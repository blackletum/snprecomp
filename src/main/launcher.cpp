// The launcher menu. The frontend's default one only offers the first
// registered game, and checks a loaded ROM against that game alone, so the
// English translation's ROM was refused as "not the correct game". This one
// takes either kind of ROM, Japanese or translated, through one Load ROM (or
// Change ROM) option and has a start option for each that's loaded
// (src/main/main.cpp's supported_games), plus Add to Steam
// (src/main/steam_shortcut.cpp).
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "recompui/recompui.h"
#include "recompui/program_config.h"
#include "base/ui_launcher.h"
#include "elements/ui_label.h"
#include "util/file.h"
#include "snp_steam.h"

#include "librecomp/game.hpp"
#include "librecomp/mods.hpp"

extern std::vector<recomp::GameEntry> supported_games;

namespace {
    struct RomChoice {
        size_t game_index;
        const char* language;
    };

    // Start options are listed in this order.
    const RomChoice rom_choices[] = {
        { 1, "English" },
        { 0, "Japanese" },
    };

    void build_menu(recompui::LauncherMenu* menu);

    // Add to Steam closes Steam first when it's running, which takes a few
    // seconds: that runs on its own thread, and the launcher's update callback
    // shows the outcome.
    enum class SteamJob { Idle, Running, Done };
    std::atomic<SteamJob> steam_job{ SteamJob::Idle };
    std::mutex steam_job_mutex;
    snp::steam::Result steam_job_result;

    void show_steam_result(const snp::steam::Result& result) {
        if (!result.ok) {
            recompui::open_info_prompt("Add to Steam", result.error, "OK", nullptr);
            return;
        }
        recompui::open_choice_prompt(
            "Add to Steam",
            std::string(result.added ? "Sin and Punishment Recompiled was added to your Steam library" : "Sin and Punishment Recompiled's Steam shortcut was updated") +
                " as a non-Steam game" + (result.art > 0 ? ", with its artwork" : "") + ". Open Steam now?",
            "Open Steam", "Not now",
            [] { snp::steam::open_steam(); }, nullptr,
            recompui::ButtonStyle::Success, recompui::ButtonStyle::Tertiary, false);
    }

    void add_to_steam(snp::steam::Location location, bool close_first) {
        steam_job = SteamJob::Running;
        std::thread([location, close_first] {
            snp::steam::Result result;
            if (close_first && !snp::steam::close_steam_and_wait(20)) {
                result.error = "Steam didn't close. Close it yourself (Steam > Exit) and try again.";
            }
            else {
                result = snp::steam::add_shortcut(location);
            }
            {
                std::lock_guard lock{ steam_job_mutex };
                steam_job_result = result;
            }
            steam_job = SteamJob::Done;
        }).detach();
    }

    void on_add_to_steam() {
        if (steam_job != SteamJob::Idle) {
            return;
        }
        if (snp::steam::started_from_steam()) {
            recompui::open_info_prompt("Add to Steam", "The game was started from Steam, so it's in your library already.", "OK", nullptr);
            return;
        }
        std::optional<snp::steam::Location> location = snp::steam::locate();
        if (!location) {
            recompui::open_info_prompt("Add to Steam", "Steam wasn't found on this computer.", "OK", nullptr);
            return;
        }
        if (snp::steam::steam_running()) {
            recompui::open_choice_prompt(
                "Add to Steam",
                "Steam has to be closed while the game is added (it rewrites its list of games when it exits). "
                "Close Steam now? It can be opened again afterwards.",
                "Close Steam", "Cancel",
                [location] {
                    recompui::open_notification("Add to Steam", "Closing Steam...");
                    add_to_steam(*location, true);
                }, nullptr,
                recompui::ButtonStyle::Success, recompui::ButtonStyle::Tertiary, false);
            return;
        }
        add_to_steam(*location, false);
    }

    void update_menu(recompui::LauncherMenu*) {
        if (steam_job == SteamJob::Done) {
            snp::steam::Result result;
            {
                std::lock_guard lock{ steam_job_mutex };
                result = steam_job_result;
            }
            steam_job = SteamJob::Idle;
            show_steam_result(result);
        }
    }

    // The frontend's title, higher (at 14% rather than 25% of the height), so
    // the options fit under it.
    void place_title(recompui::LauncherMenu* menu) {
        static bool placed = false;
        if (placed) {
            return;
        }
        placed = true;
        menu->remove_default_title();
        recompui::ContextId context = recompui::get_current_context();
        recompui::Element* wrapper = context.create_element<recompui::Element>(menu, 0, "div", false);
        wrapper->set_position(recompui::Position::Absolute);
        wrapper->set_top(14.0f, recompui::Unit::Percent);
        wrapper->set_left(50.0f, recompui::Unit::Percent);
        wrapper->set_translate_2D(-50.0f, -50.0f, recompui::Unit::Percent);
        wrapper->set_width_auto();
        wrapper->set_height_auto();
        recompui::Label* title = context.create_element<recompui::Label>(wrapper, recompui::programconfig::get_program_name(), recompui::theme::Typography::Header1);
        title->set_color(recompui::theme::color::Primary);
    }

    void start_game(const recomp::GameEntry& game) {
        recompui::update_game_mod_id(game.mod_game_id);
        if (recomp::mods::game_mode_count(game.mod_game_id, false) > 0) {
            recompui::get_launcher_menu()->show_game_mode_menu(game.game_id, game.display_name, game.thumbnail_bytes);
        }
        else {
            recomp::start_game(game.game_id, {});
            recompui::hide_all_contexts();
        }
    }

    // Stores the ROM under whichever supported game it matches.
    void load_rom() {
        recompui::file::open_file_dialog([](bool success, const std::filesystem::path& path) {
            if (!success) {
                return;
            }
            bool right_game_wrong_version = false;
            for (const recomp::GameEntry& game : supported_games) {
                switch (recomp::select_rom(path, game.game_id)) {
                    case recomp::RomValidationError::Good: {
                        recompui::ContextId context = recompui::get_launcher_context_id();
                        bool opened = context.open_if_not_already();
                        build_menu(recompui::get_launcher_menu());
                        if (opened) {
                            context.close();
                        }
                        return;
                    }
                    case recomp::RomValidationError::FailedToOpen:
                        recompui::message_box("Failed to open ROM file.");
                        return;
                    case recomp::RomValidationError::NotARom:
                        recompui::message_box("This is not a valid ROM file.");
                        return;
                    case recomp::RomValidationError::IncorrectVersion:
                        right_game_wrong_version = true;
                        break;
                    default:
                        break;
                }
            }
            if (right_game_wrong_version) {
                recompui::message_box(
                    "This ROM is Sin and Punishment, but its game code is different from the Japanese release's.\n"
                    "Supported: the Japanese release (Tsumi to Batsu: Hoshi no Keishousha) and translation patches of it "
                    "that only change text and graphics, such as the English translation.");
            }
            else {
                recompui::message_box("This ROM is not Sin and Punishment.");
            }
        });
    }

    void build_menu(recompui::LauncherMenu* menu) {
        place_title(menu);
        std::vector<const RomChoice*> loaded;
        for (const RomChoice& choice : rom_choices) {
            if (recomp::is_rom_valid(supported_games[choice.game_index].game_id)) {
                loaded.push_back(&choice);
            }
        }

        // The menu belongs to the first loaded ROM's game (the Japanese one if none
        // is loaded yet); the Controls/Settings/Mods options use its mod game id,
        // which both games share.
        const recomp::GameEntry& menu_game = supported_games[loaded.empty() ? 0 : loaded[0]->game_index];
        recompui::GameOptionsMenu* options = menu->init_game_options_menu(
            menu_game.game_id, menu_game.mod_game_id, "Sin and Punishment", menu_game.thumbnail_bytes, recompui::GameOptionsMenuLayout::Center);

        // The first option is made as the menu's start option, which the launcher
        // focuses when it shows; its callback is replaced with ours.
        for (const RomChoice* choice : loaded) {
            const recomp::GameEntry& game = supported_games[choice->game_index];
            std::string title = loaded.size() == 1 ? "Start Game" : std::string("Start Game (") + choice->language + ")";
            recompui::GameOption* option = choice == loaded[0]
                ? options->add_start_game_or_load_rom_option(title, title)
                : options->add_option(title, nullptr);
            option->set_callback([&game]() { start_game(game); });
        }
        // A ROM loaded later takes the place of the one before it of its kind.
        if (loaded.empty()) {
            options->add_start_game_or_load_rom_option("Load ROM", "Load ROM")->set_callback(load_rom);
        }
        else {
            options->add_option("Change ROM", load_rom);
        }

        options->add_setup_controls_option();
        options->add_settings_option();
        options->add_mods_option();
        options->add_option("Add to Steam", on_add_to_steam);
        options->add_exit_option();

        // Lower than the frontend's 25% from the bottom, clear of the title
        // with a start option for each ROM.
        options->set_bottom(8.0f, recompui::Unit::Percent);
    }
}

namespace snp {
    void register_launcher() {
        recompui::register_launcher_init_callback(build_menu);
        recompui::register_launcher_update_callback(update_menu);
    }
}
