/**
 * ModDetail - Mod 详情组件（占位）
 * 右侧详情面板，后续补充具体字段
 */

#include "view/modDetail.hpp"

ModDetail::ModDetail() {
    inflateFromXMLRes("xml/view/modDetail.xml");
}

brls::View* ModDetail::create() {
    return new ModDetail();
}
