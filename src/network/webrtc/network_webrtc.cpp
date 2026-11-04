// WebRTC backend. A WebSocket on localhost carries the signaling needed to
// establish a data channel between the server and each client. All input then
// goes through the data channels.
//
// A layer is the input of every player for a single frame.
//
// Library callbacks run on other threads, so they only append to an inbox.
// The input queues are only touched on the calling thread.
#include <chrono>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <rtc/rtc.hpp>

#if EMSCRIPTEN
#include <emscripten.h>
#endif

#include <network_game.h>
#include <network_mode.h>

namespace {

constexpr u16 SignalingPort = 8765;
constexpr const char* SignalingAddress = "127.0.0.1";
constexpr const char* SignalingUrl = "ws://127.0.0.1:8765";
constexpr const char* InputChannelLabel = "input";

// Signaling messages are text, with fields separated by newlines. The last
// field takes up the remainder of the message, as SDP spans multiple lines.
constexpr const char* SignalClaim = "claim";
constexpr const char* SignalPlayer = "player";
constexpr const char* SignalRejected = "rejected";
constexpr const char* SignalDescription = "sdp";
constexpr const char* SignalCandidate = "candidate";
constexpr const char* SignalStart = "start";

struct Peer {
    std::shared_ptr<rtc::WebSocket> signaling;
    std::shared_ptr<rtc::PeerConnection> connection;
    std::shared_ptr<rtc::DataChannel> channel;
    // Server only. The players that this client sends input for.
    std::vector<u64> playerIdxs;
};

struct InboxMessage {
    u64 senderIdx;
    rtc::binary data;
};

struct NetworkState {
    NetworkMode mode{ NetworkMode::Server };
    // Owned by the platform.
    InputSpecification* specification{ nullptr };
    // Every player a frame needs input from. Layers are ordered the same as
    // this, so it must match across devices.
    std::vector<PlayerId> playerIds;
    // Indices within playerIds of the players on this device.
    std::vector<u64> localPlayerIdxs;
    u64 playerCount{ 0 };
    u32 wireSize{ 0 };
    bool synced{ false };

    // Indexed by player. The front of each is the next layer given to the game.
    std::vector<std::queue<PlayerData>> inputs;
    // Indexed by player. The buttons of the last input received, as they
    // aren't sent over the network.
    std::vector<std::bitset<MaxButtonTypeCount>> lastButtonsDown;
    // Server only. Input received, but not yet part of a sent layer.
    std::vector<std::queue<PlayerData>> unsent;

    // Guards everything below, which is accessed by library callbacks.
    std::mutex mutex;
    // Server: one per client. Index 0 is the server, which goes unused.
    // Client: only index 0 is used, which is the connection to the server.
    std::vector<Peer> peers;
    std::vector<InboxMessage> inbox;
    // Server only. Keeps sockets alive until the client says what it wants.
    std::vector<std::shared_ptr<rtc::WebSocket>> pendingSignaling;
    u64 nextPeerIdx{ 1 };
    u64 nextPlayerIdx{ 0 };
    // Client only. How many players to claim from the server.
    u64 claimCount{ 0 };
    // Client only. Sockets of earlier attempts can still report events, which must be ignored.
    u64 connectAttempt{ 0 };
    // What devices must agree on. A client that doesn't match the server is rejected.
    std::string signature;
    bool rejected{ false };
    bool started{ false };
    bool signalingLost{ false };
    // Client only. Claimed players are consecutive, starting from this one.
    std::optional<u64> assignedPlayerIdx;
};

// Intentionally never destroyed, as library threads may run callbacks during exit.
NetworkState& State() {
    static NetworkState* state = new NetworkState();
    return *state;
}

void SleepMilliseconds(u32 milliseconds) {
#if EMSCRIPTEN
    // Callbacks only run once control is yielded to the browser.
    emscripten_sleep(milliseconds);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
#endif
}

// Splits off the text before the first newline.
std::string TakeField(std::string& message) {
    size_t end = message.find('\n');
    if (end == std::string::npos) {
        std::string field = std::move(message);
        message.clear();
        return field;
    }
    std::string field = message.substr(0, end);
    message.erase(0, end + 1);
    return field;
}

void AddToInbox(u64 senderIdx, rtc::message_variant message) {
    if (!std::holds_alternative<rtc::binary>(message)) return;
    NetworkState& state = State();
    std::lock_guard lock(state.mutex);
    state.inbox.push_back({ senderIdx, std::move(std::get<rtc::binary>(message)) });
}

// Sends this device's half of the handshake over the given signaling socket.
std::shared_ptr<rtc::PeerConnection> CreateConnection(std::shared_ptr<rtc::WebSocket> signaling) {
    auto connection = std::make_shared<rtc::PeerConnection>(rtc::Configuration{});
    std::weak_ptr<rtc::WebSocket> weakSignaling = signaling;
    connection->onLocalDescription([weakSignaling](rtc::Description description) {
        if (auto socket = weakSignaling.lock()) {
            socket->send(std::string(SignalDescription) + "\n" + description.typeString() + "\n" + std::string(description));
        }
    });
    connection->onLocalCandidate([weakSignaling](rtc::Candidate candidate) {
        if (auto socket = weakSignaling.lock()) {
            socket->send(std::string(SignalCandidate) + "\n" + candidate.mid() + "\n" + candidate.candidate());
        }
    });
    return connection;
}

// Applies a description or candidate. Returns false if it's neither.
bool ApplyRemoteSignal(rtc::PeerConnection& connection, const std::string& type, std::string& message) {
    if (type == SignalDescription) {
        std::string descriptionType = TakeField(message);
        connection.setRemoteDescription(rtc::Description(message, descriptionType));
        return true;
    }
    if (type == SignalCandidate) {
        std::string mid = TakeField(message);
        connection.addRemoteCandidate(rtc::Candidate(message, mid));
        return true;
    }
    return false;
}

void ReceiveFromPlayer(u64 playerIdx, PlayerData input) {
    NetworkState& state = State();
    input.m_inputButtonPrevDown = state.lastButtonsDown[playerIdx];
    state.lastButtonsDown[playerIdx] = input.m_inputButtonDown;
    state.inputs[playerIdx].push(input);
    if (state.mode == NetworkMode::Server) {
        state.unsent[playerIdx].push(input);
    }
}

// Server only. Sends out every layer that every player has contributed to.
void SendCompleteLayers() {
    NetworkState& state = State();
    while (true) {
        for (const auto& unsent : state.unsent) {
            if (unsent.empty()) return;
        }

        rtc::binary layer(state.wireSize * state.playerCount);
        for (u64 playerIdx = 0; playerIdx < state.playerCount; playerIdx++) {
            state.specification->WriteBuffer(layer.data() + playerIdx * state.wireSize, state.unsent[playerIdx].front());
            state.unsent[playerIdx].pop();
        }

        std::lock_guard lock(state.mutex);
        for (u64 peerIdx = 1; peerIdx < state.nextPeerIdx; peerIdx++) {
            if (!state.peers[peerIdx].channel->send(layer.data(), layer.size())) {
                LOG_ERROR("Failed to send input layer to client " << peerIdx);
            }
        }
    }
}

// Moves everything received over the network into the input queues.
void DrainInbox() {
    NetworkState& state = State();
    std::vector<InboxMessage> messages;
    {
        std::lock_guard lock(state.mutex);
        messages.swap(state.inbox);
    }

    for (const InboxMessage& message : messages) {
        // Clients send the input of their own players, whereas the server sends a whole layer.
        const std::vector<u64>* serverPlayerIdxs = state.mode == NetworkMode::Server ? &state.peers[message.senderIdx].playerIdxs : nullptr;
        u64 expectedPlayers = serverPlayerIdxs ? serverPlayerIdxs->size() : state.playerCount;
        if (message.data.size() != state.wireSize * expectedPlayers) {
            LOG_ERROR("Dropped input message of unexpected size " << message.data.size());
            continue;
        }

        for (u64 i = 0; i < expectedPlayers; i++) {
            PlayerData input{ };
            state.specification->ReadBuffer(message.data.data() + i * state.wireSize, input);
            ReceiveFromPlayer(serverPlayerIdxs ? (*serverPlayerIdxs)[i] : i, input);
        }
    }

    if (state.mode == NetworkMode::Server) {
        SendCompleteLayers();
    }
}

#if !EMSCRIPTEN
// Kept for the lifetime of the program, like the rest of the state.
rtc::WebSocketServer* signalingServer = nullptr;

// Only one device can bind the port, so this is what decides who the server is.
bool TryHostSignaling() {
    rtc::WebSocketServerConfiguration config;
    config.port = SignalingPort;
    config.bindAddress = SignalingAddress;
    try {
        signalingServer = new rtc::WebSocketServer(config);
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

// Server only. Gives a client the players it asked for. Returns its peer, if there's room.
std::optional<u64> AcceptClaim(u64 count, const std::string& signature, std::shared_ptr<rtc::WebSocket> signaling) {
    NetworkState& state = State();
    std::lock_guard lock(state.mutex);
    if (signature != state.signature) {
        LOG_ERROR("Rejected a client, as its input specification or players differ. Expected ["
            << state.signature << "] but got [" << signature << "]");
        signaling->send(std::string(SignalRejected));
        return std::nullopt;
    }
    if (count == 0 || state.nextPlayerIdx + count > state.playerCount) {
        LOG_ERROR("Rejected a client claiming " << count << " players, as there isn't enough left");
        signaling->close();
        return std::nullopt;
    }

    u64 peerIdx = state.nextPeerIdx++;
    u64 firstPlayerIdx = state.nextPlayerIdx;
    state.nextPlayerIdx += count;

    Peer& peer = state.peers[peerIdx];
    for (u64 i = 0; i < count; i++) {
        peer.playerIdxs.push_back(firstPlayerIdx + i);
    }
    peer.signaling = signaling;
    peer.connection = CreateConnection(signaling);
    // Creating the channel is what produces the offer, so handlers have to be set by now.
    peer.channel = peer.connection->createDataChannel(InputChannelLabel);
    peer.channel->onMessage([peerIdx](rtc::message_variant message) {
        AddToInbox(peerIdx, std::move(message));
    });
    signaling->send(std::string(SignalPlayer) + "\n" + std::to_string(firstPlayerIdx));
    return peerIdx;
}

void SyncAsServer() {
    NetworkState& state = State();
    if (state.nextPlayerIdx == state.playerCount) return;

    signalingServer->onClient([](std::shared_ptr<rtc::WebSocket> signaling) {
        NetworkState& state = State();
        {
            std::lock_guard lock(state.mutex);
            state.pendingSignaling.push_back(signaling);
        }

        std::weak_ptr<rtc::WebSocket> weakSignaling = signaling;
        auto peerIdx = std::make_shared<std::optional<u64>>();
        signaling->onMessage([peerIdx, weakSignaling](rtc::message_variant data) {
            if (!std::holds_alternative<std::string>(data)) return;
            std::string message = std::move(std::get<std::string>(data));
            std::string type = TakeField(message);
            if (type == SignalClaim) {
                u64 count = std::stoull(TakeField(message));
                if (auto socket = weakSignaling.lock()) *peerIdx = AcceptClaim(count, message, socket);
                return;
            }
            if (!peerIdx->has_value()) return;

            NetworkState& state = State();
            std::lock_guard lock(state.mutex);
            Peer& peer = state.peers[**peerIdx];
            if (peer.connection) {
                ApplyRemoteSignal(*peer.connection, type, message);
            }
        });
    });

    while (true) {
        {
            std::lock_guard lock(state.mutex);
            bool allOpen = state.nextPlayerIdx == state.playerCount;
            for (u64 peerIdx = 1; peerIdx < state.nextPeerIdx; peerIdx++) {
                const Peer& peer = state.peers[peerIdx];
                allOpen = allOpen && peer.channel && peer.channel->isOpen();
            }
            if (allOpen) {
                for (u64 peerIdx = 1; peerIdx < state.nextPeerIdx; peerIdx++) {
                    state.peers[peerIdx].signaling->send(std::string(SignalStart));
                }
                return;
            }
        }
        SleepMilliseconds(1);
    }
}
#endif

// Client only. Forgets any progress, and tries reaching the server.
void BeginConnectingToServer() {
    NetworkState& state = State();
    std::lock_guard lock(state.mutex);
    Peer& server = state.peers[0];
    server.channel.reset();
    server.connection.reset();
    server.signaling = std::make_shared<rtc::WebSocket>();
    state.signalingLost = false;
    u64 attempt = ++state.connectAttempt;

    auto markLost = [attempt]() {
        NetworkState& state = State();
        std::lock_guard lock(state.mutex);
        if (attempt == state.connectAttempt) state.signalingLost = true;
    };
    std::weak_ptr<rtc::WebSocket> weakSignaling = server.signaling;
    std::string claim = std::string(SignalClaim) + "\n" + std::to_string(state.claimCount) + "\n" + state.signature;
    server.signaling->onOpen([weakSignaling, claim]() {
        if (auto socket = weakSignaling.lock()) socket->send(claim);
    });
    server.signaling->onClosed(markLost);
    server.signaling->onError([markLost](std::string error) { markLost(); });
    server.signaling->onMessage([attempt](rtc::message_variant data) {
        if (!std::holds_alternative<std::string>(data)) return;
        std::string message = std::move(std::get<std::string>(data));
        std::string type = TakeField(message);

        NetworkState& state = State();
        std::lock_guard lock(state.mutex);
        if (attempt != state.connectAttempt) return;
        Peer& server = state.peers[0];
        if (type == SignalPlayer) {
            state.assignedPlayerIdx = std::stoull(message);
        }
        else if (type == SignalRejected) {
            state.rejected = true;
        }
        else if (type == SignalStart) {
            state.started = true;
        }
        else {
            if (!server.connection) {
                server.connection = CreateConnection(server.signaling);
                server.connection->onDataChannel([](std::shared_ptr<rtc::DataChannel> channel) {
                    channel->onMessage([](rtc::message_variant message) {
                        AddToInbox(0, std::move(message));
                    });
                    NetworkState& state = State();
                    std::lock_guard lock(state.mutex);
                    state.peers[0].channel = channel;
                });
            }
            ApplyRemoteSignal(*server.connection, type, message);
        }
    });
    server.signaling->open(SignalingUrl);
}

// Returns whether the server accepted this device.
bool SyncAsClient() {
    NetworkState& state = State();
    BeginConnectingToServer();

    while (true) {
        bool retry = false;
        {
            std::lock_guard lock(state.mutex);
            if (state.rejected) return false;
            const Peer& server = state.peers[0];
            if (state.started && state.assignedPlayerIdx && server.channel && server.channel->isOpen()) {
                for (u64 i = 0; i < state.claimCount; i++) {
                    state.localPlayerIdxs.push_back(*state.assignedPlayerIdx + i);
                }
                return true;
            }
            // The server may simply not be up yet.
            retry = state.signalingLost && !state.started;
        }
        if (retry) {
            SleepMilliseconds(100);
            BeginConnectingToServer();
        }
        SleepMilliseconds(1);
    }
}

} // namespace

void SyncConnection(InputSpecification* specification, std::vector<GatheringScheme> schemes, std::vector<PlayerId> playerIds) {
    NetworkState& state = State();
    if (state.synced) return;
    TRY_LOG_RETURN(specification, "SyncConnection needs an input specification");
    TRY_LOG_RETURN(!schemes.empty(), "SyncConnection needs at least one scheme");
    TRY_LOG_RETURN(schemes.size() <= playerIds.size(), "SyncConnection given more schemes than players");

    PlayerData empty{ };
    std::array<std::byte, MaxBufferSize> buffer;
    state.wireSize = specification->WriteBuffer(buffer.data(), empty);
    state.specification = specification;

    state.signature = specification->GetSignature() + " synced";
    for (PlayerId playerId : playerIds) {
        state.signature += " " + std::to_string(playerId);
    }

    state.playerIds = std::move(playerIds);
    state.playerCount = state.playerIds.size();
    state.inputs.assign(state.playerCount, { });
    state.lastButtonsDown.assign(state.playerCount, { });
    state.unsent.assign(state.playerCount, { });

    // If this device has every player, there is nobody to connect to and no port is needed.
    bool isServer = schemes.size() == state.playerCount;
#if !EMSCRIPTEN
    // Browsers cannot listen for incoming connections, so are always clients.
    isServer = isServer || TryHostSignaling();
#endif
    state.mode = isServer ? NetworkMode::Server : NetworkMode::Client;

    if (isServer) {
        // Each client has at least one player, so this is the most there can be.
        state.peers.resize(state.playerCount + 1);
        for (u64 i = 0; i < schemes.size(); i++) {
            state.localPlayerIdxs.push_back(i);
        }
        state.nextPlayerIdx = schemes.size();
#if !EMSCRIPTEN
        SyncAsServer();
#endif
    }
    else {
        state.peers.resize(1);
        state.claimCount = schemes.size();
        if (!SyncAsClient()) {
            // An expected outcome rather than a bug here, so this shouldn't break into the debugger.
            LOG_ERROR("Server rejected this device, as its input specification or players differ");
            return;
        }
        TRY_LOG_RETURN(state.localPlayerIdxs.back() < state.playerCount, "Server assigned a player this device doesn't know of");
    }
    state.synced = true;

    for (u64 i = 0; i < schemes.size(); i++) {
        PlayerId playerId = state.playerIds[state.localPlayerIdxs[i]];
        for (const auto& [inputId, info] : schemes[i].buttons) {
            state.specification->AddButtonGatheringSpec(playerId, inputId, info);
        }
        for (const auto& [inputId, info] : schemes[i].inputs2d) {
            state.specification->AddInput2dGatheringSpec(playerId, inputId, info);
        }
    }
}

bool IsConnected() {
    return State().synced;
}

bool SendInput(std::span<const PlayerData> inputs) {
    NetworkState& state = State();
    if (!state.synced) return false;

    DrainInbox();
    rtc::binary message(state.wireSize * state.localPlayerIdxs.size());
    for (u64 i = 0; i < state.localPlayerIdxs.size(); i++) {
        u64 playerIdx = state.localPlayerIdxs[i];
        u64 inputIdx = state.specification->GetPlayerIdx(state.playerIds[playerIdx]);
        TRY_LOG_RETURN(inputIdx < inputs.size(), "Not given the input of every player", false);
        if (state.mode == NetworkMode::Server) {
            ReceiveFromPlayer(playerIdx, inputs[inputIdx]);
        }
        else {
            state.specification->WriteBuffer(message.data() + i * state.wireSize, inputs[inputIdx]);
        }
    }

    if (state.mode == NetworkMode::Server) {
        SendCompleteLayers();
        return true;
    }

    // Not added to the local queues, as it comes back as part of a whole layer.
    std::lock_guard lock(state.mutex);
    const auto& channel = state.peers[0].channel;
    return channel && channel->isOpen() && channel->send(message.data(), message.size());
}

std::vector<PlayerData> GetInputs() {
    NetworkState& state = State();
    if (!state.synced) return { };

    DrainInbox();
    for (const auto& input : state.inputs) {
        if (input.empty()) return { };
    }

    std::vector<PlayerData> layer;
    layer.reserve(state.playerCount);
    for (auto& input : state.inputs) {
        layer.push_back(input.front());
        input.pop();
    }
    return layer;
}
