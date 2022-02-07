#include <intercept.hpp>



#include <string>
#include <sstream>


#include "../addons/main/script_version.hpp"

#include "prometheus/client_metric.h"
#include "prometheus/counter.h"
#include "prometheus/exposer.h"
#include "prometheus/family.h"
#include "prometheus/registry.h"

using namespace intercept;

namespace pm = prometheus;

using SQFPar = game_value_parameter;

void prettyDiagLog(std::string msg) {
    intercept::sqf::diag_log(sqf::text(msg));
}

void prettyDiagLogInfo(std::string info) {
    prettyDiagLog("[GRAD] (prism) INFO: " + info);
}

int intercept::api_version() {
    return INTERCEPT_SDK_API_VERSION;
}

void intercept::register_interfaces() {}

void intercept::pre_init() {
    std::stringstream strVersion;
    strVersion << "Running (" << MAJOR << "." << MINOR << "." << PATCHLVL << "." << BUILD << ")";
    prettyDiagLogInfo(strVersion.str());


}

void intercept::pre_start() {

    /*static auto grad_intercept_template_test_command =
        client::host::register_sqf_command("gradInterceptTestCommand", "Test Command", testCommand, game_data_type::BOOL, game_data_type::STRING);*/

}

const int32_t portOffset = 5;

bool keepRunning = true;

void intercept::post_init() {
    try {
        auto gamePort = (int32_t)sqf::call(sqf::compile("[] call arma3_reflection_commandline_fnc_port;"));

        if (gamePort == 0) {
            gamePort = 2302;
        }

        auto port = gamePort + portOffset;

        prettyDiagLogInfo(std::string("Using game port: ").append(std::to_string(gamePort)));
        prettyDiagLogInfo(std::string("Using port: ").append(std::to_string(port)));

        std::thread exposerThread([port]() {

            pm::Exposer exposer{ std::string("127.0.0.1:").append(std::to_string(port)) };
            auto registry = std::make_shared<pm::Registry>();


            auto& fpsGaugeFamily = pm::BuildGauge()
                .Name("diag_fps")
                .Help("Current FPS")
                .Register(*registry);

            auto& fpsGauge = fpsGaugeFamily.Add({});

            exposer.RegisterCollectable(registry);

            client::invoker_lock threadLock;
            float_t fps = 0;
            while (keepRunning)
            {
                threadLock.lock();
                fps = (float_t)sqf::diag_fps();
                threadLock.unlock();
                fpsGauge.Set(fps);
            }

            });

        exposerThread.detach();
    }
    catch (std::exception& ex) {
        prettyDiagLog(ex.what());
    }
}

void intercept::mission_ended() {
    keepRunning = false;
}

