#include "script_component.hpp"

class CfgPatches {
    class grad_intercept_template_main {
        name = "Gruppe Adler Intercept Template";
        units[] = {};
        weapons[] = {};
        requiredVersion = 1.92;
        requiredAddons[] = {"intercept_core"};
        authors[] = { "Willard" };
        url = "https://github.com/gruppe-adler/grad_intercept_template";
        VERSION_CONFIG;
    };
};
class Intercept {
    class grad {
        class grad_intercept_template {
            pluginName = "grad_intercept_template";
        };
    };
};
