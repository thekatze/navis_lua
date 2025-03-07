#pragma once

#include "./IScene.h"
#include <stack>

class EngineApi;

class SceneStack {
  public:
    SceneStack(EngineApi &api) : m_scenes(), m_api(api) {}

    template <class T, class... Args> void push(const Args &...args) {
        if (m_scenes.size() != 0) {
            m_scenes.top()->on_pause(m_api);
        }

        m_scenes.emplace(std::make_shared<T>(args...));
        m_scenes.top()->on_enter(m_api);
    }

    void pop();

    template <class T, class... Args> void swap(const Args &...args) {
        pop();
        push<T>(args...);
    }

    std::shared_ptr<IScene> current();

  private:
    std::stack<std::shared_ptr<IScene>> m_scenes;
    EngineApi &m_api;
};
