#pragma once
#include "ControllerMappings.h"
namespace e4 {
class ControllerService {
public:
    ControllerExchange exchange;
    std::atomic<int> learnRequest {-2},learned {0};
    void requestLearn(int slot) noexcept { learned.store(0); learnRequest.store(slot); }
    bool update() noexcept {
        const int request=learnRequest.exchange(-2);
        if (request!=-2) router.learn(request);
        return exchange.consume(pending,serial);
    }
    template <typename Act> void apply(Act&& act) noexcept { router.replace(pending,act); }
    ControllerRouter router;
private:
    ControllerProfile pending;
    std::uint64_t serial=0;
};
}
