#pragma once

template<typename T, void(*ReleaseFunc)(T)>
struct Guard{ 
private:
    T m_data;

public:
    T get() const {
        return m_data;
    }
    const T* data() const {
        return &m_data;
    }

    explicit Guard(T&& data) : m_data(std::move(data)) {}

    ~Guard() {
        ReleaseFunc(m_data);
    }

    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
    Guard(Guard&&) = delete;
    Guard& operator=(Guard&&) = delete;
};