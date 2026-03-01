#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/GeodeUI.hpp> // Нужно для открытия настроек

using namespace geode::prelude;

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
        "Skill issue!", "Why?", "Attempts go brrr", 
        "Controller disconnected?", "Lag!", "Oof", 
        "Nice try", "Almost...", "Gravity is hard",
        "Don't give up!", "One more time...", ":("
    };
    return messages[rand() % messages.size()].c_str();
}

// ============================================================
// SPEEDHACK
// ============================================================

class $modify(SpeedScheduler, CCScheduler) {
    void update(float dt) {
        if (Mod::get()->getSettingValue<bool>("speedhack-enabled")) {
            dt *= static_cast<float>(Mod::get()->getSettingValue<double>("speed-value"));
        }
        CCScheduler::update(dt);
    }
};

// ============================================================
// PLAY LAYER (ОСНОВНАЯ ЛОГИКА)
// ============================================================

class $modify(UltraPlayLayer, PlayLayer) {
    
    // Используем Fields для хранения переменных уровня
    // Это предотвращает баги при перезапуске
    struct Fields {
        bool m_isDead = false;          // Флаг, умер ли игрок (чтобы не считать попытки 20 раз)
        int m_sessionAttempts = 0;      // Счётчик попыток за сессию
        float m_rainbowHue = 0.0f;      // Для радуги
        float m_msgTimer = 0.0f;        // Таймер сообщения
        
        CCLabelBMFont* m_attemptLabel = nullptr;
        CCLabelBMFont* m_percentLabel = nullptr;
        CCLabelBMFont* m_deathMsgLabel = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        // Инициализация переменных
        m_fields->m_isDead = false;
        m_fields->m_sessionAttempts = 1; // Начинаем с 1-й попытки
        m_fields->m_rainbowHue = 0.0f;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        // 1. Счётчик попыток
        if (Mod::get()->getSettingValue<bool>("show-attempts")) {
            m_fields->m_attemptLabel = CCLabelBMFont::create("Att: 1", "bigFont.fnt");
            m_fields->m_attemptLabel->setScale(0.4f);
            m_fields->m_attemptLabel->setPosition({winSize.width - 5.0f, winSize.height - 5.0f});
            m_fields->m_attemptLabel->setAnchorPoint({1.0f, 1.0f});
            m_fields->m_attemptLabel->setOpacity(150);
            m_fields->m_attemptLabel->setZOrder(1000);
            this->addChild(m_fields->m_attemptLabel);
        }

        // 2. Точный процент
        if (Mod::get()->getSettingValue<bool>("percent-accuracy")) {
            m_fields->m_percentLabel = CCLabelBMFont::create("0.00%", "bigFont.fnt");
            m_fields->m_percentLabel->setScale(0.4f);
            m_fields->m_percentLabel->setPosition({winSize.width / 2.0f, winSize.height - 15.0f});
            m_fields->m_percentLabel->setAnchorPoint({0.5f, 1.0f});
            m_fields->m_percentLabel->setOpacity(200);
            m_fields->m_percentLabel->setZOrder(1000);
            this->addChild(m_fields->m_percentLabel);
        }

        // 3. Сообщения о смерти (Скрыто по умолчанию!)
        if (Mod::get()->getSettingValue<bool>("death-message-enabled")) {
            m_fields->m_deathMsgLabel = CCLabelBMFont::create("", "goldFont.fnt");
            m_fields->m_deathMsgLabel->setScale(0.6f);
            m_fields->m_deathMsgLabel->setPosition({winSize.width / 2.0f, winSize.height / 2.0f + 40.0f});
            m_fields->m_deathMsgLabel->setOpacity(0); // Скрыто!
            m_fields->m_deathMsgLabel->setVisible(false); // Точно скрыто!
            m_fields->m_deathMsgLabel->setZOrder(2000);
            this->addChild(m_fields->m_deathMsgLabel);
        }

        // 4. Кнопка настроек мода (Справа, но не в углу)
        // Используем спрайт шестеренки (или edit button)
        auto sprite = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
        sprite->setScale(0.6f);
        
        auto btn = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(UltraPlayLayer::onOpenModSettings)
        );
        
        auto menu = CCMenu::create();
        menu->addChild(btn);
        // Позиция: Справа (width - 25), Высота 70% (height * 0.7)
        menu->setPosition({winSize.width - 25.0f, winSize.height * 0.7f});
        menu->setZOrder(9999);
        this->addChild(menu);

        this->schedule(schedule_selector(UltraPlayLayer::onUltraUpdate));

        return true;
    }

    // Обработчик нажатия на кнопку
    void onOpenModSettings(CCObject*) {
        geode::openSettingsPopup(Mod::get());
    }

    // Обновление каждый кадр
    void onUltraUpdate(float dt) {
        if (!m_fields) return;
        auto player = this->m_player1;
        if (!player) return;

        // Rainbow Icon
        if (Mod::get()->getSettingValue<bool>("rainbow-icon")) {
            m_fields->m_rainbowHue += dt * 150.0f;
            if (m_fields->m_rainbowHue >= 360.0f) m_fields->m_rainbowHue -= 360.0f;
            
            ccColor3B color = hsvToRgb(m_fields->m_rainbowHue, 1.0f, 1.0f);
            player->setColor(color);
            player->setSecondColor(hsvToRgb(std::fmod(m_fields->m_rainbowHue + 180.f, 360.f), 1.f, 1.f));
            
            if (this->m_player2) {
                this->m_player2->setColor(color);
            }
        }

        // Обновление процента
        if (m_fields->m_percentLabel && Mod::get()->getSettingValue<bool>("percent-accuracy")) {
            float currentX = player->getPositionX();
            float len = this->m_levelLength;
            if (len > 0) {
                float pct = (currentX / len) * 100.0f;
                if (pct < 0) pct = 0; if (pct > 100) pct = 100;
                m_fields->m_percentLabel->setString(fmt::format("{:.2f}%", pct).c_str());
            }
        }
        
        // Таймер исчезновения сообщения
        if (m_fields->m_msgTimer > 0) {
            m_fields->m_msgTimer -= dt;
            if (m_fields->m_msgTimer <= 0) {
                 if (m_fields->m_deathMsgLabel) {
                     m_fields->m_deathMsgLabel->runAction(CCFadeOut::create(0.5f));
                 }
            }
        }

        // Wave Trail
        if (Mod::get()->getSettingValue<bool>("wave-trail-always") && player->m_waveTrail) {
            player->m_waveTrail->setVisible(true);
        }
    }

    // ЛОГИКА СМЕРТИ
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // Noclip
        if (Mod::get()->getSettingValue<bool>("noclip-enabled")) {
            return; // Просто выходим, игрок не умирает
        }

        // ПРОВЕРКА: Если уже умер, не считаем снова!
        if (m_fields->m_isDead) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }

        // Помечаем, что умерли
        m_fields->m_isDead = true;
        m_fields->m_sessionAttempts++; // +1 попытка

        // Обновляем текст попыток
        if (m_fields->m_attemptLabel) {
            m_fields->m_attemptLabel->setString(
                fmt::format("Att: {}", m_fields->m_sessionAttempts).c_str()
            );
            // Анимация увеличения
            m_fields->m_attemptLabel->stopAllActions();
            m_fields->m_attemptLabel->setScale(0.6f);
            m_fields->m_attemptLabel->runAction(CCEaseElasticOut::create(CCScaleTo::create(0.5f, 0.4f), 0.3f));
        }

        // Показываем сообщение
        if (m_fields->m_deathMsgLabel && Mod::get()->getSettingValue<bool>("death-message-enabled")) {
            m_fields->m_deathMsgLabel->setString(getRandomDeathMessage());
            m_fields->m_deathMsgLabel->setVisible(true);
            m_fields->m_deathMsgLabel->setOpacity(255);
            m_fields->m_deathMsgLabel->stopAllActions();
            
            // Анимация "Прыг"
            m_fields->m_deathMsgLabel->setScale(0.0f);
            m_fields->m_deathMsgLabel->runAction(CCEaseBackOut::create(CCScaleTo::create(0.3f, 0.6f)));
            
            m_fields->m_deathMsgLabel->setColor(hsvToRgb(rand() % 360, 0.8f, 1.0f));
            
            // Таймер до исчезновения
            m_fields->m_msgTimer = 1.5f; 
        }

        PlayLayer::destroyPlayer(player, obj);
    }

    // Сброс уровня (рестарт)
    void resetLevel() {
        PlayLayer::resetLevel();
        
        // Сбрасываем флаг смерти, чтобы можно было умереть снова
        m_fields->m_isDead = false;
        
        // Скрываем сообщение сразу
        if (m_fields->m_deathMsgLabel) {
            m_fields->m_deathMsgLabel->stopAllActions();
            m_fields->m_deathMsgLabel->setOpacity(0);
            m_fields->m_deathMsgLabel->setVisible(false);
        }
    }
};

// ============================================================
// ПАУЗА
// ============================================================

class $modify(UltraPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto label = CCLabelBMFont::create("Mod Active", "bigFont.fnt");
        label->setScale(0.4f);
        label->setPosition({winSize.width - 40.0f, 40.0f});
        label->setOpacity(100);
        this->addChild(label);
    }
};
