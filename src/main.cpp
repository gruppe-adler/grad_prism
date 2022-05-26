#include <intercept.hpp>

#ifdef WIN32
#include <Windows.h>

#include "TCHAR.h"
#include "pdh.h"
#include "psapi.h"

#else
#pragma error Currently Windows only!
#endif

#include <string>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <fmt/format.h>
#include <iostream>
#include <string>
#include <string_view>

#include <boost/algorithm/string.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "../addons/main/script_version.hpp"

#include "prometheus/client_metric.h"
#include "prometheus/counter.h"
#include "prometheus/exposer.h"
#include "prometheus/family.h"
#include "prometheus/registry.h"

#ifdef PRISM_BENCHMARK

#if WIN32
#include <Shlobj.h>
#endif

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/stopwatch.h"
#endif

using namespace intercept;

namespace pm = prometheus;
namespace fs = std::filesystem;

using SQFPar = game_value_parameter;

void prettyDiagLog(std::string msg) {
    intercept::sqf::diag_log(sqf::text(msg));
}

void prettyDiagLogInfo(std::string info) {
    prettyDiagLog("[GRAD] (prism) INFO: " + info);
}

void prettyDiagLogWarning(std::string warning) {
    prettyDiagLog("[GRAD] (prism) WARNING: " + warning);
}

void prettyDiagLogError(std::string err) {
    prettyDiagLog("[GRAD] (prism) ERROR: " + err);
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


const int32_t portOffset = 5;

bool keepRunning = false;

std::vector<std::string> configSettings = {};

prometheus::Gauge* missionTimeGauge = nullptr;
prometheus::Family<prometheus::Gauge>* missionTimeFamily = nullptr;

int getGamePort() {
#ifdef WIN32

    int nArgs = 0;
    auto args = CommandLineToArgvW(GetCommandLineW(), &nArgs);

    if (NULL == args)
    {
        prettyDiagLogInfo("CommandLineToArgvW failed");
        return 0;
    }

    std::vector<std::wstring> argsVector(args, args + nArgs);

    auto foundItem = std::find_if(argsVector.begin(), argsVector.end(), [](std::wstring wstr) {
        return wstr.starts_with(L"-port");
    });

    if (foundItem != argsVector.end()) {
        std::vector<std::wstring> wstr = {};

        boost::split(wstr, *foundItem, boost::is_any_of(L"="));

        if (wstr.size() >= 2) {
            return std::stoi(wstr[1]);
        }
    }
    return 0;
#endif
}


game_value resetMetrics(game_state& gs) {
    keepRunning = false;
    return true;
}

void arma_loop(
    prometheus::Gauge& avgFpsGauge,
    prometheus::Gauge& minFpsGauge,

    prometheus::Gauge& activeSpawnedGauge,
    prometheus::Gauge& activeExecVMedGauge,
    prometheus::Gauge& activeExecedGauge,
    prometheus::Gauge& activeexecFSMedGauge,

    prometheus::Gauge& playersWestGauge,
    prometheus::Gauge& playersEastGauge,
    prometheus::Gauge& playersIndepGauge,
    prometheus::Gauge& playersCivGauge,
    prometheus::Gauge& playersUnkGauge,
    prometheus::Gauge& playersLobbyGauge,

    prometheus::Gauge& localAiGauge,
    prometheus::Gauge& nonLocalAiGauge,

    prometheus::Gauge& allDeadMenGauge
) {
    try
    {
        client::invoker_lock threadLock(true);

        float_t avgFps = 0;
        float_t minFps = 0;

        std::vector<float> activeScripts = {};

        double_t playersWest = 0, playersEast = 0, playersIndep = 0, playersCiv = 0, playersUnk = 0, playersLobby = 0, localAi = 0, nonLocalAi = 0,
            allDeadMen = 0, time = 0;

        auto west = sqf::west(), east = sqf::east(), indep = sqf::independent(), civ = sqf::civilian(), ambie = sqf::side_ambient_life();

        while (true)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));

#ifdef PRISM_BENCHMARK
            spdlog::stopwatch sw;
#endif

            if (keepRunning) {
                threadLock.lock();
                avgFps = (float_t)sqf::diag_fps();
                minFps = (float_t)sqf::diag_fpsmin();

                auto allUnits = sqf::all_units();

                playersWest = 0, playersEast = 0, playersIndep = 0, playersCiv = 0, playersUnk = 0, localAi = 0, nonLocalAi = 0;

                for (auto& unit : allUnits)
                {
                    if (sqf::is_player(unit)) {

                        auto side = sqf::side_get(sqf::group_get(unit));
                        if (side == west) {
                            playersWest++;
                        }
                        else if (side == east) {
                            playersEast++;
                        }
                        else if (side == indep) {
                            playersIndep++;
                        }
                        else if (side == civ) {
                            playersCiv++;
                        }
                        else {
                            playersUnk++;
                        }
                    }
                    else {
                        if (sqf::local(unit)) {
                            localAi++;
                        }
                        else {
                            nonLocalAi++;
                        }
                    }
                }
                playersLobby = sqf::all_users().size() - (playersWest + playersEast + playersIndep + playersCiv + playersUnk);

                activeScripts = sqf::diag_active_scripts();

                allDeadMen = sqf::all_deadmen().size();

                time = sqf::time();

                threadLock.unlock();

#ifdef PRISM_BENCHMARK
                spdlog::info("Spent {} in arma loop (synchronous part)", sw);
#endif

                if (missionTimeGauge != nullptr) {
                    missionTimeGauge->Set(time);
                }

                avgFpsGauge.Set(avgFps);
                minFpsGauge.Set(minFps);

                if (activeScripts.size() > 3) {
                    activeSpawnedGauge.Set(activeScripts[0]);
                    activeExecVMedGauge.Set(activeScripts[1]);
                    activeExecedGauge.Set(activeScripts[2]);
                    activeexecFSMedGauge.Set(activeScripts[3]);
                }

                playersWestGauge.Set(playersWest);
                playersEastGauge.Set(playersEast);
                playersIndepGauge.Set(playersIndep);
                playersCivGauge.Set(playersCiv);
                playersUnkGauge.Set(playersUnk);
                playersLobbyGauge.Set(playersLobby);

                localAiGauge.Set(localAi);
                nonLocalAiGauge.Set(nonLocalAi);

                allDeadMenGauge.Set(allDeadMen);

            }
            else {
                avgFpsGauge.Set(0);
                minFpsGauge.Set(0);

                activeSpawnedGauge.Set(0);
                activeExecVMedGauge.Set(0);
                activeExecedGauge.Set(0);
                activeexecFSMedGauge.Set(0);

                playersWestGauge.Set(0);
                playersEastGauge.Set(0);
                playersIndepGauge.Set(0);
                playersCivGauge.Set(0);
                playersUnkGauge.Set(0);
                playersLobbyGauge.Set(0);

                localAiGauge.Set(0);
                nonLocalAiGauge.Set(0);

                allDeadMenGauge.Set(0);
            }

#ifdef PRISM_BENCHMARK
            spdlog::info("Spent {} in arma loop (complete part)", sw);
#endif

        }
    }
    catch (const std::exception& ex)
    {
        client::invoker_lock lock;
        prettyDiagLogError(ex.what());
    }
}

// https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process

bool resource_loop(
    prometheus::Gauge& cpuTotalGauge,
    prometheus::Gauge& cpuProcessGauge,

    prometheus::Gauge& memVirtTotalGauge,
    prometheus::Gauge& memVirtProcessGauge,
    prometheus::Gauge& memPhysTotalGauge,
    prometheus::Gauge& memPhysProcessGauge) {

    try
    {
#ifdef WIN32
        // CPU usage setup

        PDH_HQUERY cpuQuery;
        PDH_HCOUNTER cpuTotal;

        if (PdhOpenQuery(NULL, NULL, &cpuQuery) != ERROR_SUCCESS) {
            return false;
        }

        if (PdhAddEnglishCounter(cpuQuery, "\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal) != ERROR_SUCCESS) {
            return false;
        }
        if (PdhCollectQueryData(cpuQuery) != ERROR_SUCCESS) {
            return false;
        }

        // CPU usage by current process setup
        ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;

        SYSTEM_INFO sysInfo;
        FILETIME ftime, fsys, fuser;

        GetSystemInfo(&sysInfo);
        int numProcessors = sysInfo.dwNumberOfProcessors;

        GetSystemTimeAsFileTime(&ftime);
        memcpy(&lastCPU, &ftime, sizeof(FILETIME));

        HANDLE self = GetCurrentProcess();
        if (GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser) == 0) {
            return false;
        }
        memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
        memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));


#endif // WIN32


        while (true)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));

#ifdef PRISM_BENCHMARK
            spdlog::stopwatch sw;
#endif

#ifdef WIN32
            // CPU usage
            PDH_FMT_COUNTERVALUE counterVal;
            double cpuProcesP = -1;

            if (PdhCollectQueryData(cpuQuery) == ERROR_SUCCESS &&
                PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal) == ERROR_SUCCESS) {
                cpuTotalGauge.Set(counterVal.doubleValue);

                // CPU usage by current process
                FILETIME ftime, fsys, fuser;
                ULARGE_INTEGER now, sys, user;
                double percent;

                GetSystemTimeAsFileTime(&ftime);
                memcpy(&now, &ftime, sizeof(FILETIME));

                if (GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser)) {
                    memcpy(&sys, &fsys, sizeof(FILETIME));
                    memcpy(&user, &fuser, sizeof(FILETIME));
                    percent = (sys.QuadPart - lastSysCPU.QuadPart) +
                        (user.QuadPart - lastUserCPU.QuadPart);
                    percent /= max(1, (now.QuadPart - lastCPU.QuadPart));
                    percent /= numProcessors;
                    lastCPU = now;
                    lastUserCPU = user;
                    lastSysCPU = sys;

                    cpuProcesP = percent * 100;
                }
            }

            cpuProcessGauge.Set(cpuProcesP);

            // Memory
            double memVirtTotal = -1, memVirtProc = -1, memPhysTotal = -1, memPhysProc = -1;

            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            if (GlobalMemoryStatusEx(&memInfo)) {

                memVirtTotal = memInfo.ullTotalPageFile - memInfo.ullAvailPageFile;

                PROCESS_MEMORY_COUNTERS_EX pmc;
                if (GetProcessMemoryInfo(self, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
                    memVirtProc = pmc.PrivateUsage;

                    memPhysTotal = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
                    memPhysProc = pmc.WorkingSetSize;
                }
            }

            memVirtTotalGauge.Set(memVirtTotal);
            memVirtProcessGauge.Set(memVirtProc);
            memPhysTotalGauge.Set(memPhysTotal);
            memPhysProcessGauge.Set(memPhysProc);

#endif // WIN32

#ifdef PRISM_BENCHMARK
            spdlog::info("Spent {} in resource loop", sw);
#endif
        }
    }
    catch (const std::exception& ex)
    {
        client::invoker_lock lock;
        prettyDiagLogError(ex.what());
        return false;
    }

    return true;
}


void intercept::pre_start() {

#ifdef PRISM_BENCHMARK
#if WIN32
    std::filesystem::path a3_log_path;
    PWSTR path_tmp;

    auto get_folder_path_ret = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path_tmp);

    if (get_folder_path_ret == S_OK) {
        a3_log_path = path_tmp;
    }
    CoTaskMemFree(path_tmp);

    a3_log_path = a3_log_path / "Arma 3";

#endif
    auto async_file = spdlog::basic_logger_mt<spdlog::async_factory>(
        "async_file_logger",
        fmt::format("{}/grad_prism_benchmark_{:%Y-%m-%d_%H-%M-%S}.log", a3_log_path.string(), std::chrono::system_clock::now()));
    spdlog::flush_every(std::chrono::seconds(10));
    spdlog::set_default_logger(async_file);
    spdlog::info("grad_prism_benchmark init");

#endif

    if (!fs::is_directory("grad_prism")) {
        fs::create_directory("grad_prism");
    }

    if (!fs::is_directory("grad_prism/logs")) {
        fs::create_directory("grad_prism/logs");
    }

    boost::property_tree::ptree pt;
    auto path = std::filesystem::path("grad_prism_config.ini").string();
    try {
        boost::property_tree::ini_parser::read_ini(path, pt);

        for (auto& item : pt) {
            configSettings.push_back(item.first);
            configSettings.push_back(item.second.data());
        }
    }
    catch (boost::property_tree::ini_parser_error ex) {
        prettyDiagLogWarning("Couldn't parse grad_prism_config.ini, writing a new one");
        boost::property_tree::ini_parser::write_ini(path, pt);
    }

    try {
        auto gamePort = getGamePort();

        if (gamePort == 0) {
            gamePort = 1337;
        }

        auto port = gamePort + portOffset;

        prettyDiagLogInfo(std::string("Using game port: ").append(std::to_string(gamePort)));
        prettyDiagLogInfo(std::string("Using port: ").append(std::to_string(port)));

        std::thread exposerThread([port]() {

            auto civetConfig = std::vector<std::string>{
                "listening_ports",  std::to_string(port),
                "error_log_file", fmt::format("grad_prism/logs/error_{}.log", port),
                "access_log_file", fmt::format("grad_prism/logs/access_{}.log", port),
                /*"ssl_certificate", "/path/to/ssl_cert.pem",
                "ssl_ca_file", "/path/to/calist.pem",
                "ssl_verify_peer", "yes",*/
                "num_threads", "2",
            };

            civetConfig.insert(civetConfig.end(), configSettings.begin(), configSettings.end());

            pm::Exposer exposer{ civetConfig };
            auto registry = std::make_shared<pm::Registry>();

            auto& fpsGaugeFamily = pm::BuildGauge()
                .Name("fps")
                .Help("Current framerate calculated over last 16 frames")
                .Register(*registry);
            auto& avgFpsGauge = fpsGaugeFamily.Add({ {"type", "avg"} });
            auto& minFpsGauge = fpsGaugeFamily.Add({ {"type", "min"} });

            auto& activeScriptsGaugeFamily = pm::BuildGauge()
                .Name("activeScripts")
                .Help("Number of currently running scripts")
                .Register(*registry);
            auto& activeSpawnedGauge = activeScriptsGaugeFamily.Add({ {"type", "spawned"} });
            auto& activeExecVMedGauge = activeScriptsGaugeFamily.Add({ {"type", "execVMed"} });
            auto& activeExecedGauge = activeScriptsGaugeFamily.Add({ {"type", "execed"} });
            auto& activeexecFSMedGauge = activeScriptsGaugeFamily.Add({ {"type", "execFSMed"} });

            auto& playersGaugeFamily = pm::BuildGauge()
                .Name("players")
                .Help("Count of players")
                .Register(*registry);
            auto& playersWestGauge = playersGaugeFamily.Add({ { "side", "west" } });
            auto& playersEastGauge = playersGaugeFamily.Add({ { "side", "east" } });
            auto& playersIndepGauge = playersGaugeFamily.Add({ { "side", "independent" } });
            auto& playersCivGauge = playersGaugeFamily.Add({ { "side", "civilian" } });
            auto& playersUnkGauge = playersGaugeFamily.Add({ { "side", "unknown"} });
            auto& playersLobbyGauge = playersGaugeFamily.Add({ { "side", "lobby"} });

            auto& aiGaugeFamily = pm::BuildGauge()
                .Name("ai")
                .Help("Count of AI")
                .Register(*registry);
            auto& localAiGauge = aiGaugeFamily.Add({ { "type", "local" } });
            auto& nonLocalAiGauge = aiGaugeFamily.Add({ { "type", "non_local" } });

            auto& allDeadMenFamily = pm::BuildGauge()
                .Name("allDeadMen")
                .Help("A list of dead units including agents")
                .Register(*registry);
            auto& allDeadMenGauge = allDeadMenFamily.Add({});

            auto& cpuGaugeFamily = pm::BuildGauge()
                .Name("cpu")
                .Help("Current CPU usage")
                .Register(*registry);

            auto& cpuTotalGauge = cpuGaugeFamily.Add({ { "type", "total_usage" } });
            auto& cpuProcessGauge = cpuGaugeFamily.Add({ { "type", "process_usage" } });

            auto& memGaugeFamily = pm::BuildGauge()
                .Name("mem")
                .Help("Current Memory usage")
                .Register(*registry);

            auto& memVirtTotalGauge = memGaugeFamily.Add({ { "type", "virtual_total_usage" } });
            auto& memVirtProcessGauge = memGaugeFamily.Add({ { "type", "virtual_process_usage" } });
            auto& memPhysTotalGauge = memGaugeFamily.Add({ { "type", "physical_total_usage" } });
            auto& memPhysProcessGauge = memGaugeFamily.Add({ { "type", "physical_process_usage" } });

            missionTimeFamily = &pm::BuildGauge()
                .Name("time")
                .Help("Time spent in mission")
                .Register(*registry);

            exposer.RegisterCollectable(registry);

            std::thread armaLoopThread(arma_loop,
                std::ref(avgFpsGauge), std::ref(minFpsGauge),
                std::ref(activeSpawnedGauge), std::ref(activeExecVMedGauge), std::ref(activeExecedGauge), std::ref(activeexecFSMedGauge),
                std::ref(playersWestGauge), std::ref(playersEastGauge), std::ref(playersIndepGauge), std::ref(playersCivGauge),
                std::ref(playersUnkGauge), std::ref(playersLobbyGauge),
                std::ref(localAiGauge), std::ref(nonLocalAiGauge),
                std::ref(allDeadMenGauge)
            );
            armaLoopThread.detach();

            std::thread ressourceLoopThread(resource_loop,
                std::ref(cpuTotalGauge), std::ref(cpuProcessGauge),
                std::ref(memVirtTotalGauge), std::ref(memVirtProcessGauge),
                std::ref(memPhysTotalGauge), std::ref(memPhysProcessGauge));
            ressourceLoopThread.detach();

            while(true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });

        exposerThread.detach();
    }
    catch (std::exception& ex) {
        prettyDiagLog(ex.what());
    }

    static auto grad_intercept_template_test_command =
        client::host::register_sqf_command("gradPrismReset", "Resets all metrics", resetMetrics, game_data_type::NOTHING);


    sqf::add_mission_event_handler("MPEnded", sqf::compile("gradPrismReset;"));

}

void intercept::post_init() {
    keepRunning = true;
    if (missionTimeFamily != nullptr) {
        missionTimeGauge = &missionTimeFamily->Add({ { "mission" , sqf::briefing_name() }});
    }
}

void intercept::mission_ended() {
    keepRunning = false;
    if (missionTimeGauge != nullptr) {
        missionTimeGauge->Set(0);
    }
    missionTimeGauge = nullptr;
}
