// The launcher menu. The frontend's default one only offers the first
// registered game, and checks a loaded ROM against that game alone, so the
// English translation's ROM was refused as "not the correct game". This one
// takes either ROM through one Load ROM option and has a start option for
// each ROM that's loaded (src/main/main.cpp's supported_games).
#include <string>
#include <vector>

#include "recompui/recompui.h"
#include "base/ui_launcher.h"
#include "util/file.h"

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
                    "This ROM is Sin and Punishment, but not a supported version.\n"
                    "Supported: the Japanese release (Tsumi to Batsu: Hoshi no Keishousha) or the fan English translation. "
                    "See the README for their SHA-1 hashes.");
            }
            else {
                recompui::message_box("This ROM is not Sin and Punishment.");
            }
        });
    }

    void build_menu(recompui::LauncherMenu* menu) {
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
        // Once both ROMs are loaded there's nothing else to load (and the menu would
        // run into the title).
        if (loaded.empty()) {
            options->add_start_game_or_load_rom_option("Load ROM", "Load ROM")->set_callback(load_rom);
        }
        else if (loaded.size() < std::size(rom_choices)) {
            options->add_option("Load Another ROM", load_rom);
        }

        options->add_setup_controls_option();
        options->add_settings_option();
        options->add_mods_option();
        options->add_exit_option();
    }
}

namespace snp {
    void register_launcher() {
        recompui::register_launcher_init_callback(build_menu);
    }
}
