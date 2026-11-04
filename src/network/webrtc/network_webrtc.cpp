// WebRTC backend. A WebSocket on localhost carries the signaling needed to
// establish a data channel between the server and each client. All input then
// goes through the data channels.
//
// A layer is the input of every player for a single frame.
//
// Library callbacks run on other threads, so they only append to an inbox.
// The queues handed to the game are only touched on the calling thread.
#include <chrono>
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

#include <input.h>
#include <network_mode.h>

namespace {

constexpr u16 SignalingPort = 8765;
constexpr const char* SignalingAddress = "127.0.0.1";
constexpr const char* SignalingUrl = "ws://127.0.0.1:8765";
constexpr const char* InputChannelLabel = "input";

// Signaling messages are text, with fields separated by newlines. The last
// field takes up the remainder of the message, as SDP spans multiple lines.
constexpr const char* SignalPlayer = "player";
constexpr const char* SignalDescription = "sdp";
constexpr const char* SignalCandidate = "candidate";
constexpr const char* SignalStart = "start";

struct Peer {
    std::shared_ptr<rtc::WebSocket> signaling;
    std::shared_ptr<rtc::PeerConnection> connection;
    std::shared_ptr<rtc::DataChannel> channel;
};

struct InboxMessage {
    u64 senderIdx;
    rtc::binary data;
};

struct NetworkState {
    NetworkMode mode{ NetworkMode::Server };
    std::optional<InputSpecification> specification;
    u64 localPlayerIdx{ 0 };
    u64 playerCount{ 0 };
    u32 wireSize{ 0 };
    bool synced{ false };

    // Indexed by player. Exposed to the game.
    std::vector<std::queue<std::optional<PlayerData>>> inputs;
    // Indexed by player. The buttons of the last input received, as they
    // aren't sent over the network.
    std::vector<std::bitset<MaxButtonTypeCount>> lastButtonsDown;
    // Server only. Input received, but not yet part of a sent layer.
    std::vector<std::queue<PlayerData>> unsent;

    // Guards everything below, which is accessed by library callbacks.
    std::mutex mutex;
    // Server: indexed by player. Index 0 is the server, which goes unused.
    // Client: only index 0 is used, which is the connection to the server.
    std::vector<Peer> peers;
    std::vector<InboxMessage> inbox;
    u64 nextClientIdx{ 1 };
    bool started{ false };
    bool signalingLost{ false };
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
        for (u64 peerIdx = 1; peerIdx < state.peers.size(); peerIdx++) {
            if (!state.peers[peerIdx].channel->send(layer.data(), layer.size())) {
                LOG_ERROR("Failed to send input layer to player " << peerIdx);
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
        // Clients send a single player's input, whereas the server sends a whole layer.
        u64 expectedSize = state.mode == NetworkMode::Server ? state.wireSize : state.wireSize * state.playerCount;
        if (message.data.size() != expectedSize) {
            LOG_ERROR("Dropped input message of unexpected size " << message.data.size());
            continue;
        }

        if (state.mode == NetworkMode::Server) {
            PlayerData input{ };
            state.specification->ReadBuffer(message.data.data(), input);
            ReceiveFromPlayer(message.senderIdx, input);
        }
        else {
            for (u64 playerIdx = 0; playerIdx < state.playerCount; playerIdx++) {
                PlayerData input{ };
                state.specification->ReadBuffer(message.data.data() + playerIdx * state.wireSize, input);
                ReceiveFromPlayer(playerIdx, input);
            }
        }
    }

    if (state.mode == NetworkMode::Server) {
        SendCompleteLayers();
    }
}

#if !EMSCRIPTEN
// Server only. Called once the signaling socket of a client is usable.
void BeginClientHandshake(u64 playerIdx, std::shared_ptr<rtc::WebSocket> signaling) {
    NetworkState& state = State();
    std::lock_guard lock(state.mutex);
    Peer& peer = state.peers[playerIdx];
    peer.connection = CreateConnection(signaling);
    // Creating the channel is what produces the offer, so handlers have to be set by now.
    peer.channel = peer.connection->createDataChannel(InputChannelLabel);
    peer.channel->onMessage([playerIdx](rtc::message_variant message) {
        AddToInbox(playerIdx, std::move(message));
    });
    signaling->send(std::string(SignalPlayer) + "\n" + std::to_string(playerIdx));
}

void SyncAsServer() {
    NetworkState& state = State();
    u64 clientCount = state.playerCount - 1;
    if (clientCount == 0) return;

    rtc::WebSocketServerConfiguration config;
    config.port = SignalingPort;
    config.bindAddress = SignalingAddress;
    // Kept for the lifetime of the program, like the rest of the state.
    static rtc::WebSocketServer* server = new rtc::WebSocketServer(config);
    server->onClient([](std::shared_ptr<rtc::WebSocket> signaling) {
        NetworkState& state = State();
        u64 playerIdx;
        {
            std::lock_guard lock(state.mutex);
            if (state.nextClientIdx >= state.playerCount) {
                LOG_ERROR("Rejected a connection, as all players are already connected");
                signaling->close();
                return;
            }
            playerIdx = state.nextClientIdx++;
            state.peers[playerIdx].signaling = signaling;
        }

        std::weak_ptr<rtc::WebSocket> weakSignaling = signaling;
        signaling->onOpen([playerIdx, weakSignaling]() {
            if (auto socket = weakSignaling.lock()) BeginClientHandshake(playerIdx, socket);
        });
        signaling->onMessage([playerIdx](rtc::message_variant data) {
            if (!std::holds_alternative<std::string>(data)) return;
            std::string message = std::move(std::get<std::string>(data));
            std::string type = TakeField(message);
            NetworkState& state = State();
            std::lock_guard lock(state.mutex);
            if (state.peers[playerIdx].connection) {
                ApplyRemoteSignal(*state.peers[playerIdx].connection, type, message);
            }
        });
    });

    while (true) {
        {
            std::lock_guard lock(state.mutex);
            bool allOpen = true;
            for (u64 peerIdx = 1; peerIdx < state.peers.size(); peerIdx++) {
                const Peer& peer = state.peers[peerIdx];
                allOpen = allOpen && peer.channel && peer.channel->isOpen();
            }
            if (allOpen) {
                for (u64 peerIdx = 1; peerIdx < state.peers.size(); peerIdx++) {
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

    auto markLost = []() {
        NetworkState& state = State();
        std::lock_guard lock(state.mutex);
        state.signalingLost = true;
    };
    server.signaling->onClosed(markLost);
    server.signaling->onError([markLost](std::string error) { markLost(); });
    server.signaling->onMessage([](rtc::message_variant data) {
        if (!std::holds_alternative<std::string>(data)) return;
        std::string message = std::move(std::get<std::string>(data));
        std::string type = TakeField(message);

        NetworkState& state = State();
        std::lock_guard lock(state.mutex);
        Peer& server = state.peers[0];
        if (type == SignalPlayer) {
            state.assignedPlayerIdx = std::stoull(message);
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

void SyncAsClient() {
    NetworkState& state = State();
    BeginConnectingToServer();

    while (true) {
        bool retry = false;
        {
            std::lock_guard lock(state.mutex);
            const Peer& server = state.peers[0];
            if (state.started && state.assignedPlayerIdx && server.channel && server.channel->isOpen()) {
                state.localPlayerIdx = *state.assignedPlayerIdx;
                return;
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

void SetNetworkMode(NetworkMode mode) {
    State().mode = mode;
}

u64 GetLocalPlayerIdx() {
    return State().localPlayerIdx;
}

void SetInputSpecification(InputSpecification specification) {
    NetworkState& state = State();
    state.playerCount = specification.GetPlayerCount(0);
    PlayerData empty{ };
    std::array<std::byte, MaxBufferSize> buffer;
    state.wireSize = specification.WriteBuffer(buffer.data(), empty);
    state.specification.emplace(std::move(specification));

    state.inputs.assign(state.playerCount, { });
    state.lastButtonsDown.assign(state.playerCount, { });
    state.unsent.assign(state.playerCount, { });
}

void SyncConnection() {
    NetworkState& state = State();
    TRY_LOG_RETURN(state.specification.has_value(), "SetInputSpecification must be called before SyncConnection");
    if (state.synced) return;

    if (state.mode == NetworkMode::Server) {
#if EMSCRIPTEN
        // Browsers cannot listen for incoming connections.
        FAULT_LOG("Web builds can only connect as a client");
#else
        state.peers.resize(state.playerCount);
        SyncAsServer();
#endif
    }
    else {
        state.peers.resize(1);
        SyncAsClient();
    }
    state.synced = true;
}

bool SendInput(PlayerData input) {
    NetworkState& state = State();
    if (!state.synced) return false;

    DrainInbox();
    if (state.mode == NetworkMode::Server) {
        ReceiveFromPlayer(state.localPlayerIdx, input);
        SendCompleteLayers();
        return true;
    }

    // Not added to the local queue, as it comes back as part of a whole layer.
    std::array<std::byte, MaxBufferSize> buffer;
    u32 size = state.specification->WriteBuffer(buffer.data(), input);
    std::lock_guard lock(state.mutex);
    const auto& channel = state.peers[0].channel;
    return channel && channel->isOpen() && channel->send(buffer.data(), size);
}

const std::queue<std::optional<PlayerData>>& GetInputs(PlayerId playerId) {
    static const std::queue<std::optional<PlayerData>> empty;
    NetworkState& state = State();
    if (!state.specification.has_value()) return empty;

    DrainInbox();
    u64 playerIdx = state.specification->GetPlayerIdx(playerId);
    TRY_LOG_RETURN(playerIdx < state.playerCount, "Unknown player id", empty);
    return state.inputs[playerIdx];
}

bool PopInputs(int count) {
    NetworkState& state = State();
    if (count < 0 || !state.specification.has_value()) return false;

    DrainInbox();
    // Input is received in order and never skipped, so having enough
    // entries for every player means all of those layers are complete.
    for (const auto& input : state.inputs) {
        if (input.size() < (size_t)count) return false;
    }
    for (auto& input : state.inputs) {
        for (int i = 0; i < count; i++) input.pop();
    }
    return true;
}
