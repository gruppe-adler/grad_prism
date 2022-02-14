#include "script_component.hpp"

class CfgPatches {
    class grad_prism_main {
        name = "Gruppe Adler PRISM";
        units[] = {};
        weapons[] = {};
        requiredVersion = 1.92;
        requiredAddons[] = {
            "intercept_core"
        };
        authors[] = { "Willard" };
        url = "https://github.com/gruppe-adler/grad_prism";
        VERSION_CONFIG;
    };
};
class Intercept {
    class grad {
        class grad_prism {
            pluginName = "grad_prism";
        };
    };
};
