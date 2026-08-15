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

void ShellState::setHeaderState(HeaderState state) {
    if (m_state.headerState == state) return;

    m_state.headerState = std::move(state);
    m_stateChangedEvent.fire(m_state);
}

void ShellState::setHeaderTitle(TitleState title) {
    if (m_state.headerState.title == title) return;

    m_state.headerState.setTitle(std::move(title));
    m_stateChangedEvent.fire(m_state);
}

void ShellState::setHeaderContentTitle(std::string contentTitle) {
    if (m_state.headerState.contentTitle == contentTitle) return;

    m_state.headerState.setContentTitle(std::move(contentTitle));
    m_stateChangedEvent.fire(m_state);
}

void ShellState::setIndexText(std::string indexText) {
    if (m_state.indexText == indexText) return;

    m_state.indexText = std::move(indexText);
    m_stateChangedEvent.fire(m_state);
}

void ShellState::setFooterBackgroundTheme(std::string themeKey) {
    if (m_state.footerBackgroundTheme == themeKey) return;

    m_state.footerBackgroundTheme = std::move(themeKey);
    m_stateChangedEvent.fire(m_state);
}
