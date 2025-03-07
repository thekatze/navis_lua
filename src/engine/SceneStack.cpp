#include "./SceneStack.h"
#include "IScene.h"

void SceneStack::pop() {
    m_scenes.top()->on_exit(m_api);
    m_scenes.pop();
    if (m_scenes.size() != 0) {
        m_scenes.top()->on_resume(m_api);
    }
}

std::shared_ptr<IScene> SceneStack::current() {
    if (m_scenes.size() <= 0)
        return nullptr;

    return m_scenes.top();
}
