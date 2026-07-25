/**
 * ShellState - 当前页面提供给全局外壳的显示状态
 */

#include "ui/core/shellState.hpp"
#include <utility>

ShellState::~ShellState() = default;

const ShellStateData& ShellState::getState() const {
    return m_state;
}

brls::Event<const ShellStateData&>* ShellState::getStateChangedEvent() {
    return &m_stateChangedEvent;
}

void ShellState::setTitle(std::string title) {
    if (m_state.title == title) return;

    m_state.title = std::move(title);
    m_stateChangedEvent.fire(m_state);
}

void ShellState::setSubtitle(std::string subtitle) {
    if (m_state.subtitle == subtitle) return;

    m_state.subtitle = std::move(subtitle);
    m_stateChangedEvent.fire(m_state);
}

void ShellState::setIndexText(std::string indexText) {
    if (m_state.indexText == indexText) return;

    m_state.indexText = std::move(indexText);
    m_stateChangedEvent.fire(m_state);
}
