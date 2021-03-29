#include <intercept.hpp>

#include <boost/algorithm/algorithm.hpp>
#include <boost/algorithm/string.hpp>

#include <string>
#include <sstream>


#include "../addons/main/script_version.hpp"

using namespace intercept;
using SQFPar = game_value_parameter;

namespace ba = boost::algorithm;

void prettyDiagLog(std::string msg) {
    intercept::sqf::diag_log(sqf::text(msg));
}

void prettyDiagLogInfo(std::string info) {
    prettyDiagLog("[GRAD] (intercept_template) INFO: " + info);
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

game_value testCommand(game_state& gs, SQFPar right_arg) {
    if (right_arg.type_enum() == GameDataType::STRING) {
        prettyDiagLogInfo(ba::to_upper_copy(static_cast<std::string>(right_arg)));
        return true;
    }
    else {
        gs.set_script_error(game_state::game_evaluator::evaluator_error_type::type, "string expected"sv);
        return false;
    }
}

void intercept::pre_start() {

    static auto grad_intercept_template_test_command =
        client::host::register_sqf_command("gradInterceptTestCommand", "Test Command", testCommand, game_data_type::BOOL, game_data_type::STRING);

}

void intercept::post_init() {

}

