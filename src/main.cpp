#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;

// ============================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================

static float g_rainbowHue = 0.0f;
static int g_attemptCount = 0;
static CCLabelBMFont* g_attemptLabel = nullptr;
static CCLabelBMFont* g_percentLabel = nullptr;
static CCLabelBMFont* g_deathMsgLabel = nullptr;
static float g_deathMsgTimer = 0.0f;

// ============================================================
// УТИЛИТЫ
// ============================================================

ccColor3B hsvToRgb(float h, float s, float v) {
    float c = v * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r = 0, g = 0, b = 0;

    if (h < 60)       { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }

    return {
        static_cast<GLubyte>((r + m) * 255),
        static_cast<GLubyte>((g + m) * 255),
        static_cast<GLubyte>((b + m) * 255)
    };
}

const char* getRandomDeathMessage() {
    static const std::vector<std::string> messages = {
        "RIP! Maybe next time...",
        "Skill issue detected!",
        "The floor is lava... literally.",
        "Did you even try?",
        "Error 404: Skill not found",
        "Oof... that hurt.",
        "You died. Again. Shocking.",
        "Pro tip: Don't die!",
        "GG... oh wait, you died.",
        "Bruh moment #9999",
        "Task failed successfully!",
        "Not like this...",
        "F in the chat",
        "Respawning your dignity...",
        "That spike had a family!",
        "Ouch! Right in the pixels!",
        "Loading better reflexes...",
        "Have you tried NOT dying?",
        "Achievement Unlocked: Death!",
        "Insert coin to continue",
        "You were THIS close!",
        "Pain. Just pain."
    };

    int idx = rand() % messages.size();
    return messages[idx].c_str();
}

// ============================================================
// SPEEDHACK
// ============================================================

class $modify(SpeedScheduler, CCScheduler) {

    void update(float dt) {
        if (Mod::get()->getSettingValue<bool>("speedhack-enabled")) {
            float speed = static_cast<float>(Mod::get()->getSettingValue<double>("speed-value"));
            dt *= speed;
        }

        CCScheduler::update(dt);
    }
};

// ============================================================
// PLAY LAYER
// ============================================================

class $modify(UltraPlayLayer, PlayLayer) {

    struct Fields {
        bool m_isDead = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        g_attemptCount = 0;
        g_attemptLabel = nullptr;
        g_percentLabel = nullptr;
        g_deathMsgLabel = nullptr;
        g_deathMsgTimer = 0.0f;
        m_fields->m_isDead = false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // --- Счётчик попыток ---
        if (Mod::get()->getSettingValue<bool>("show-attempts")) {
            g_attemptLabel = CCLabelBMFont::create("Attempts: 0", "bigFont.fnt");
            g_attemptLabel->setScale(0.35f);
            g_attemptLabel->setPosition({winSize.width - 10.0f, winSize.height - 15.0f});
            g_attemptLabel->setAnchorPoint({1.0f, 1.0f});
            g_attemptLabel->setOpacity(180);
            g_attemptLabel->setZOrder(1000);
            this->addChild(g_attemptLabel);
        }

        // --- Точный процент ---
        if (Mod::get()->getSettingValue<bool>("percent-accuracy")) {
            g_percentLabel = CCLabelBMFont::create("0.00%", "bigFont.fnt");
            g_percentLabel->setScale(0.4f);
            g_percentLabel->setPosition({winSize.width / 2.0f, winSize.height - 15.0f});
            g_percentLabel->setAnchorPoint({0.5f, 1.0f});
            g_percentLabel->setOpacity(200);
            g_percentLabel->setZOrder(1000);
            this->addChild(g_percentLabel);
        }

        // --- Лейбл для сообщений смерти ---
        if (Mod::get()->getSettingValue<bool>("death-message-enabled")) {
            g_deathMsgLabel = CCLabelBMFont::create("", "goldFont.fnt");
            g_deathMsgLabel->setScale(0.55f);
            g_deathMsgLabel->setPosition({winSize.width / 2.0f, winSize.height / 2.0f + 50.0f});
            g_deathMsgLabel->setAnchorPoint({0.5f, 0.5f});
            g_deathMsgLabel->setOpacity(0);
            g_deathMsgLabel->setZOrder(2000);
            this->addChild(g_deathMsgLabel);
        }

        this->schedule(schedule_selector(UltraPlayLayer::onUltraUpdate));

        return true;
    }

    void onUltraUpdate(float dt) {
        auto player = this->m_player1;
        if (!player) return;

        // --- Rainbow иконка ---
        if (Mod::get()->getSettingValue<bool>("rainbow-icon")) {
            g_rainbowHue += dt * 120.0f;
            if (g_rainbowHue >= 360.0f) g_rainbowHue -= 360.0f;

            ccColor3B color1 = hsvToRgb(g_rainbowHue, 1.0f, 1.0f);
            ccColor3B color2 = hsvToRgb(std::fmod(g_rainbowHue + 180.0f, 360.0f), 1.0f, 1.0f);

            player->setColor(color1);
            player->setSecondColor(color2);

            if (this->m_player2) {
                this->m_player2->setColor(color2);
                this->m_player2->setSecondColor(color1);
            }
        }

        // --- Обновление точного процента ---
        if (g_percentLabel && Mod::get()->getSettingValue<bool>("percent-accuracy")) {
            float playerX = player->getPositionX();
            float levelLength = this->m_levelLength;

            if (levelLength > 0) {
                float accuratePercent = (playerX / levelLength) * 100.0f;
                if (accuratePercent > 100.0f) accuratePercent = 100.0f;
                if (accuratePercent < 0.0f) accuratePercent = 0.0f;

                char buf[32];
                snprintf(buf, sizeof(buf), "%.2f%%", accuratePercent);
                g_percentLabel->setString(buf);

                if (accuratePercent < 30.0f) {
                    g_percentLabel->setColor({255, 100, 100});
                } else if (accuratePercent < 60.0f) {
                    g_percentLabel->setColor({255, 255, 100});
                } else if (accuratePercent < 90.0f) {
                    g_percentLabel->setColor({100, 255, 100});
                } else {
                    g_percentLabel->setColor({100, 200, 255});
                }
            }
        }

        // --- Таймер сообщения смерти ---
        if (g_deathMsgLabel && g_deathMsgTimer > 0.0f) {
            g_deathMsgTimer -= dt;
            if (g_deathMsgTimer <= 0.0f) {
                g_deathMsgLabel->setOpacity(0);
                g_deathMsgTimer = 0.0f;
            } else if (g_deathMsgTimer < 0.5f) {
                g_deathMsgLabel->setOpacity(static_cast<GLubyte>(g_deathMsgTimer / 0.5f * 255));
            }
        }

        // --- Wave Trail Always ---
        if (Mod::get()->getSettingValue<bool>("wave-trail-always")) {
            if (player->m_waveTrail) {
                player->m_waveTrail->setVisible(true);
            }
        }
    }

    // ---- Используем destroyPlayer из PlayLayer, не из PlayerObject ----
    void destroyPlayer(PlayerObject* player, GameObject* obj) {

        // Noclip — просто не вызываем оригинал
        if (Mod::get()->getSettingValue<bool>("noclip-enabled")) {
            // Не умираем
            return;
        }

        g_attemptCount++;

        // Обновляем счётчик
        if (g_attemptLabel && Mod::get()->getSettingValue<bool>("show-attempts")) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Attempts: %d", g_attemptCount);
            g_attemptLabel->setString(buf);

            g_attemptLabel->setScale(0.5f);
            g_attemptLabel->runAction(CCEaseElasticOut::create(
                CCScaleTo::create(0.5f, 0.35f), 0.3f
            ));

            if (g_attemptCount < 10) {
                g_attemptLabel->setColor({200, 255, 200});
            } else if (g_attemptCount < 50) {
                g_attemptLabel->setColor({255, 255, 150});
            } else if (g_attemptCount < 100) {
                g_attemptLabel->setColor({255, 180, 100});
            } else {
                g_attemptLabel->setColor({255, 100, 100});
            }
        }

        // Показать сообщение смерти
        if (g_deathMsgLabel && Mod::get()->getSettingValue<bool>("death-message-enabled")) {
            g_deathMsgLabel->setString(getRandomDeathMessage());
            g_deathMsgLabel->setOpacity(255);
            g_deathMsgTimer = 2.0f;

            g_deathMsgLabel->setScale(0.0f);
            g_deathMsgLabel->runAction(CCEaseElasticOut::create(
                CCScaleTo::create(0.6f, 0.55f), 0.4f
            ));

            g_deathMsgLabel->setColor(hsvToRgb(
                static_cast<float>(rand() % 360), 0.7f, 1.0f
            ));
        }

        PlayLayer::destroyPlayer(player, obj);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        m_fields->m_isDead = false;

        if (g_deathMsgLabel) {
            g_deathMsgLabel->setOpacity(0);
            g_deathMsgTimer = 0.0f;
        }
    }

    void onQuit() {
        g_attemptLabel = nullptr;
        g_percentLabel = nullptr;
        g_deathMsgLabel = nullptr;

        PlayLayer::onQuit();
    }
};

// ============================================================
// PAUSE LAYER
// ============================================================

class $modify(UltraPauseLayer, PauseLayer) {

    void customSetup() {
        PauseLayer::customSetup();

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto modLabel = CCLabelBMFont::create("UltraHack Pack v1.0", "goldFont.fnt");
        modLabel->setScale(0.4f);
        modLabel->setPosition({winSize.width / 2.0f, 25.0f});
        modLabel->setOpacity(120);
        modLabel->setZOrder(100);
        this->addChild(modLabel);

        std::string status = "";

        if (Mod::get()->getSettingValue<bool>("noclip-enabled")) {
            status += "[Noclip] ";
        }
        if (Mod::get()->getSettingValue<bool>("speedhack-enabled")) {
            char buf[32];
            snprintf(buf, sizeof(buf), "[Speed: %.1fx] ",
                static_cast<float>(Mod::get()->getSettingValue<double>("speed-value")));
            status += buf;
        }
        if (Mod::get()->getSettingValue<bool>("rainbow-icon")) {
            status += "[Rainbow] ";
        }

        if (!status.empty()) {
            auto statusLabel = CCLabelBMFont::create(status.c_str(), "chatFont.fnt");
            statusLabel->setScale(0.6f);
            statusLabel->setPosition({winSize.width / 2.0f, 45.0f});
            statusLabel->setColor({100, 255, 100});
            statusLabel->setOpacity(180);
            statusLabel->setZOrder(100);
            this->addChild(statusLabel);
        }

        char attemptBuf[64];
        snprintf(attemptBuf, sizeof(attemptBuf), "Session Attempts: %d", g_attemptCount);
        auto attemptPauseLabel = CCLabelBMFont::create(attemptBuf, "chatFont.fnt");
        attemptPauseLabel->setScale(0.5f);
        attemptPauseLabel->setPosition({winSize.width / 2.0f, 60.0f});
        attemptPauseLabel->setColor({255, 200, 100});
        attemptPauseLabel->setOpacity(160);
        attemptPauseLabel->setZOrder(100);
        this->addChild(attemptPauseLabel);
    }
};

// ============================================================
// END LEVEL LAYER
// ============================================================

class $modify(UltraEndLevel, EndLevelLayer) {

    void customSetup() {
        EndLevelLayer::customSetup();

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        static const std::vector<std::string> winMessages = {
            "You're a LEGEND!",
            "GG! Absolutely cracked!",
            "EZ clap, no cap!",
            "Built different!",
            "Top 1% gamer right here!",
            "Certified pro moment!",
            "That was CLEAN!",
            "Mom, get the camera!",
            "Victory royale!",
            "Sheeeesh!"
        };

        int idx = rand() % winMessages.size();

        auto winLabel = CCLabelBMFont::create(winMessages[idx].c_str(), "goldFont.fnt");
        winLabel->setScale(0.0f);
        winLabel->setPosition({winSize.width / 2.0f, winSize.height / 2.0f + 80.0f});
        winLabel->setZOrder(500);
        this->addChild(winLabel);

        winLabel->runAction(CCSequence::create(
            CCDelayTime::create(0.5f),
            CCEaseElasticOut::create(CCScaleTo::create(0.8f, 0.6f), 0.3f),
            nullptr
        ));

        winLabel->runAction(CCRepeatForever::create(
            CCSequence::create(
                CCTintTo::create(0.3f, 255, 100, 100),
                CCTintTo::create(0.3f, 255, 255, 100),
                CCTintTo::create(0.3f, 100, 255, 100),
                CCTintTo::create(0.3f, 100, 255, 255),
                CCTintTo::create(0.3f, 100, 100, 255),
                CCTintTo::create(0.3f, 255, 100, 255),
                nullptr
            )
        ));

        char buf[128];
        snprintf(buf, sizeof(buf), "Total session deaths: %d", g_attemptCount);

        auto deathsLabel = CCLabelBMFont::create(buf, "chatFont.fnt");
        deathsLabel->setScale(0.5f);
        deathsLabel->setPosition({winSize.width / 2.0f, winSize.height / 2.0f + 60.0f});
        deathsLabel->setColor({200, 200, 255});
        deathsLabel->setZOrder(500);
        deathsLabel->setOpacity(0);
        this->addChild(deathsLabel);

        deathsLabel->runAction(CCSequence::create(
            CCDelayTime::create(1.5f),
            CCFadeIn::create(0.5f),
            nullptr
        ));
    }
};
