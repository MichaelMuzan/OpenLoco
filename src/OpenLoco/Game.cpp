#include "Game.h"
#include "Audio/Audio.h"
#include "CompanyManager.h"
#include "Config.h"
#include "GameCommands/GameCommands.h"
#include "GameException.hpp"
#include "GameState.h"
#include "Input.h"
#include "Interop/Interop.hpp"
#include "Localisation/StringIds.h"
#include "MultiPlayer.h"
#include "Objects/ObjectIndex.h"
#include "OpenLoco.h"
#include "S5/S5.h"
#include "SceneManager.h"
#include "Title.h"
#include "Ui/ProgressBar.h"
#include "Ui/WindowManager.h"
#include "Ui/WindowType.h"

namespace OpenLoco::Game
{
    static loco_global<uint8_t, 0x00508F08> _game_command_nest_level;
    static loco_global<GameCommands::LoadOrQuitMode, 0x0050A002> _savePromptType;

    // TODO: make accessible from Environment
    static loco_global<char[257], 0x0050B1CF> _pathSavesSinglePlayer;
    static loco_global<char[257], 0x0050B2EC> _pathSavesTwoPlayer;
    static loco_global<char[257], 0x0050B406> _pathScenarios;
    static loco_global<char[257], 0x0050B518> _pathLandscapes;

    static loco_global<char[256], 0x0050B745> _currentScenarioFilename;

    static loco_global<char[512], 0x0112CE04> _savePath;

    // 0x0046DB4C
    void sub_46DB4C()
    {
        call(0x0046DB4C); // draw preview map
    }

    using Ui::Windows::PromptBrowse::browse_type;

    static bool openBrowsePrompt(string_id titleId, browse_type type, const char* filter)
    {
        Audio::pauseSound();
        setPauseFlag(1 << 2);
        Gfx::invalidateScreen();
        Gfx::render();

        bool confirm = Ui::Windows::PromptBrowse::open(type, &_savePath[0], filter, titleId);

        Audio::unpauseSound();
        Ui::processMessagesMini();
        unsetPauseFlag(1 << 2);
        Gfx::invalidateScreen();
        Gfx::render();

        return confirm;
    }

    // 0x004416FF
    bool loadSaveGameOpen()
    {
        if (!isNetworked())
            strncpy(&_savePath[0], &_pathSavesSinglePlayer[0], std::size(_savePath));
        else
            strncpy(&_savePath[0], &_pathSavesTwoPlayer[0], std::size(_savePath));

        return openBrowsePrompt(StringIds::title_prompt_load_game, browse_type::load, S5::filterSV5);
    }

    // 0x004417A7
    bool loadLandscapeOpen()
    {
        strncpy(&_savePath[0], &_pathLandscapes[0], std::size(_savePath));

        return openBrowsePrompt(StringIds::title_prompt_load_landscape, browse_type::load, S5::filterSC5);
    }

    // 0x00441843
    bool saveSaveGameOpen()
    {
        strncpy(&_savePath[0], &_currentScenarioFilename[0], std::size(_savePath));

        return openBrowsePrompt(StringIds::title_prompt_save_game, browse_type::save, S5::filterSV5);
    }

    // 0x004418DB
    bool saveScenarioOpen()
    {
        auto path = fs::u8path(&_pathScenarios[0]).parent_path() / S5::getOptions().scenarioName;
        strncpy(&_savePath[0], path.u8string().c_str(), std::size(_savePath));
        strncat(&_savePath[0], S5::extensionSC5, std::size(_savePath));

        return openBrowsePrompt(StringIds::title_prompt_save_scenario, browse_type::save, S5::filterSC5);
    }

    // 0x00441993
    bool saveLandscapeOpen()
    {
        S5::getOptions().scenarioFlags &= ~(1 << 0);
        if (hasFlags(1u << 0))
        {
            S5::getOptions().scenarioFlags |= (1 << 0);
            sub_46DB4C();
        }

        auto path = fs::u8path(&_pathLandscapes[0]).parent_path() / S5::getOptions().scenarioName;
        strncpy(&_savePath[0], path.u8string().c_str(), std::size(_savePath));
        strncat(&_savePath[0], S5::extensionSC5, std::size(_savePath));

        return openBrowsePrompt(StringIds::title_prompt_save_landscape, browse_type::save, S5::filterSC5);
    }

    // 0x004424CE
    static bool sub_4424CE()
    {
        registers regs;
        call(0x004424CE);
        return regs.eax != 0;
    }

    // 0x0043BFF8
    void loadGame()
    {
        GameCommands::do_21(1, 0);
        Input::toolCancel();

        if (isEditorMode())
        {
            if (Game::loadLandscapeOpen())
            {
                // 0x0043C087
                auto path = fs::u8path(&_savePath[0]).replace_extension(S5::extensionSC5);
                std::strncpy(&_currentScenarioFilename[0], path.u8string().c_str(), std::size(_currentScenarioFilename));

                if (sub_4424CE())
                {
                    resetScreenAge();
                    throw GameException::Interrupt;
                }
            }
        }
        else if (!isNetworked())
        {
            if (Game::loadSaveGameOpen())
            {
                // 0x0043C033
                auto path = fs::u8path(&_savePath[0]).replace_extension(S5::extensionSV5);
                std::strncpy(&_currentScenarioFilename[0], path.u8string().c_str(), std::size(_currentScenarioFilename));

                if (S5::load(path, 0))
                {
                    resetScreenAge();
                    throw GameException::Interrupt;
                }
            }
        }
        else if (isNetworked())
        {
            // 0x0043C0DB
            if (CompanyManager::getControllingId() == CompanyManager::getUpdatingCompanyId())
            {
                MultiPlayer::setFlag(MultiPlayer::flags::flag_4);
                MultiPlayer::setFlag(MultiPlayer::flags::flag_3);
            }
        }

        // 0x0043C0D1
        Gfx::invalidateScreen();
    }

    // 0x0043C182
    void quitGame()
    {
        _game_command_nest_level = 0;

        // Path for networked games; untested.
        if (isNetworked())
        {
            clearScreenFlag(ScreenFlags::networked);
            auto playerCompanyId = CompanyManager::getControllingId();
            auto previousUpdatingId = CompanyManager::getUpdatingCompanyId();
            CompanyManager::setUpdatingCompanyId(playerCompanyId);

            Ui::WindowManager::closeAllFloatingWindows();

            CompanyManager::setUpdatingCompanyId(previousUpdatingId);
            setScreenFlag(ScreenFlags::networked);

            // If the other party is leaving the game, go back to the title screen.
            if (playerCompanyId != previousUpdatingId)
            {
                // 0x0043C1CD
                addr<0x00F25428, uint32_t>() = 0;
                clearScreenFlag(ScreenFlags::networked);
                clearScreenFlag(ScreenFlags::networkHost);
                addr<0x00508F0C, uint32_t>() = 0;
                CompanyManager::setControllingId(CompanyId(0));
                CompanyManager::setSecondaryPlayerId(CompanyId::null);

                Gfx::invalidateScreen();
                ObjectManager::loadIndex();

                Ui::WindowManager::close(Ui::WindowType::options);
                Ui::WindowManager::close(Ui::WindowType::companyFaceSelection);
                Ui::WindowManager::close(Ui::WindowType::objectSelection);

                clearScreenFlag(ScreenFlags::editor);
                Audio::pauseSound();
                Audio::unpauseSound();

                if (Input::hasFlag(Input::Flags::flag5))
                {
                    Input::sub_407231();
                    Input::resetFlag(Input::Flags::flag5);
                }

                Title::start();

                Ui::Windows::Error::open(StringIds::error_the_other_player_has_exited_the_game, StringIds::null);

                throw GameException::Interrupt;
            }
        }

        // 0x0043C1C8
        exitCleanly();
    }

    // 0x0043C0FD
    void returnToTitle()
    {
        if (isNetworked())
        {
            Ui::WindowManager::closeAllFloatingWindows();
        }

        Ui::WindowManager::close(Ui::WindowType::options);
        Ui::WindowManager::close(Ui::WindowType::companyFaceSelection);
        Ui::WindowManager::close(Ui::WindowType::objectSelection);
        Ui::WindowManager::close(Ui::WindowType::saveGamePrompt);

        clearScreenFlag(ScreenFlags::editor);
        Audio::pauseSound();
        Audio::unpauseSound();

        if (Input::hasFlag(Input::Flags::flag5))
        {
            Input::sub_407231();
            Input::resetFlag(Input::Flags::flag5);
        }

        Title::start();

        throw GameException::Interrupt;
    }

    // 0x0043C427
    void confirmSaveGame()
    {
        Input::toolCancel();

        if (isEditorMode())
        {
            if (Game::saveLandscapeOpen())
            {
                if (saveLandscape())
                {
                    // load landscape
                    GameCommands::do_21(2, 0);
                }
            }
        }
        else if (!isNetworked())
        {
            if (Game::saveSaveGameOpen())
            {
                // 0x0043C446
                auto path = fs::u8path(&_savePath[0]).replace_extension(S5::extensionSV5);
                std::strncpy(&_currentScenarioFilename[0], path.u8string().c_str(), std::size(_currentScenarioFilename));

                uint32_t flags = 0;
                if (Config::get().flags & Config::Flags::exportObjectsWithSaves)
                    flags = S5::SaveFlags::packCustomObjects;

                if (!S5::save(path, flags))
                    Ui::Windows::Error::open(StringIds::error_game_save_failed, StringIds::null);
                else
                    GameCommands::do_21(2, 0);
            }
        }
        else
        {
            // 0x0043C511
            GameCommands::do_72();
            MultiPlayer::setFlag(MultiPlayer::flags::flag_2);

            switch (_savePromptType)
            {
                case GameCommands::LoadOrQuitMode::loadGamePrompt:
                    MultiPlayer::setFlag(MultiPlayer::flags::flag_13); // intend to load?
                    break;
                case GameCommands::LoadOrQuitMode::returnToTitlePrompt:
                    MultiPlayer::setFlag(MultiPlayer::flags::flag_14); // intend to return to title?
                    break;
                case GameCommands::LoadOrQuitMode::quitGamePrompt:
                    MultiPlayer::setFlag(MultiPlayer::flags::flag_15); // intend to quit game?
                    break;
            }
        }

        // 0x0043C411
        Gfx::invalidateScreen();
    }

    bool saveLandscape()
    {
        // 0x0043C4B3
        auto path = fs::u8path(&_savePath[0]).replace_extension(S5::extensionSC5);
        std::strncpy(&_currentScenarioFilename[0], path.u8string().c_str(), std::size(_currentScenarioFilename));

        bool saveResult = !S5::save(path, S5::SaveFlags::scenario);
        if (saveResult)
            Ui::Windows::Error::open(StringIds::landscape_save_failed, StringIds::null);

        return saveResult;
    }

    uint32_t getFlags()
    {
        return getGameState().flags;
    }

    void setFlags(uint32_t flags)
    {
        getGameState().flags = flags;
    }

    bool hasFlags(uint32_t flags)
    {
        return (getFlags() & flags) != 0;
    }

    void removeFlags(uint32_t flags)
    {
        setFlags(getFlags() & ~flags);
    }
}
