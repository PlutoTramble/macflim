
#ifndef SRC_FRAMEGENERATOR_HPP
#define SRC_FRAMEGENERATOR_HPP

#include <coroutine>


template<typename video_frame, typename audio_frame>
struct framegenerator {
    struct promise_type {
        video_frame v_frame = nullptr;
        audio_frame a_frame = nullptr;

        framegenerator get_return_object() {
            return framegenerator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_void() {}

        std::suspend_always yield_value(std::tuple<video_frame, audio_frame> packet) {
            std::tie(v_frame, a_frame) = packet;
            return {};
        }

        void unhandled_exception() {
            std::terminate();
        }
    };

    std::coroutine_handle<promise_type> handle;

    framegenerator(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~framegenerator() {
        destroy();
    }

    // No copy
    framegenerator(const framegenerator&) = delete;
    framegenerator& operator=(const framegenerator&) = delete;

    framegenerator& operator=(framegenerator&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }

    void destroy() {
        if (handle)
            handle.destroy();
    }

    bool next() {
        if (!handle.done()) {
            handle.resume();
            return true;
        }
        return false;
    }

    std::tuple<video_frame, audio_frame> get_value() {
        return {handle.promise().v_frame, handle.promise().a_frame};
    }
};

#endif //SRC_FRAMEGENERATOR_HPP
