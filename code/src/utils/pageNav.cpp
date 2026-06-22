#include "utils/pageNav.hpp"
#include <functional>
#include <memory>
#include <vector>

namespace {

void slide(brls::View* viewA, brls::View* viewB, float dx, float dy, std::function<void()> done) {
    if (viewA) {
        viewA->setTranslationX(dx);
        viewA->setTranslationY(dy);
    }

    brls::sync([viewA, viewB, dx, dy, done = std::move(done)] {
        static brls::Animatable anim(0);
        anim.reset(0);
        anim.addStep(1.0f, 250, brls::EasingFunction::cubicOut);
        anim.setTickCallback([viewA, viewB, dx, dy] {
            float t = anim.getValue();
            if (viewA) {
                viewA->setTranslationX(dx * (1.0f - t));
                viewA->setTranslationY(dy * (1.0f - t));
            }
            if (viewB) {
                viewB->setTranslationX(-dx * t);
                viewB->setTranslationY(-dy * t);
            }
        });
        anim.setEndCallback([viewA, viewB, done](bool) {
            if (viewA) {
                viewA->setTranslationX(0);
                viewA->setTranslationY(0);
            }
            if (viewB) {
                viewB->setTranslationX(0);
                viewB->setTranslationY(0);
            }
            if (done) done();
        });
        anim.start();
    });
}

void animate(brls::View* viewA, brls::View* viewB, pageNav::Anim anim, std::function<void()> done) {
    switch (anim) {
        case pageNav::Anim::SLIDE_LEFT:  slide(viewA, viewB, -1280, 0, std::move(done)); break;
        case pageNav::Anim::SLIDE_RIGHT: slide(viewA, viewB,  1280, 0, std::move(done)); break;
        case pageNav::Anim::SLIDE_DOWN:  slide(viewA, viewB,  0,  720, std::move(done)); break;
        case pageNav::Anim::SLIDE_UP:    slide(viewA, viewB,  0, -720, std::move(done)); break;
        default: break;
    }
}

} // anonymous namespace

namespace pageNav {

void push(brls::Activity* activity, Anim anim) {
    if (anim == Anim::NONE) {
        brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
        return;
    }

    auto stack = brls::Application::getActivitiesStack();
    auto* oldContent = stack.empty() ? nullptr : stack.back()->getContentView();
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);

    auto* newContent = activity->getContentView();
    activity->setInFadeAnimation(true);
    brls::Application::blockInputs();

    auto done = [activity] {
        activity->setInFadeAnimation(false);
        brls::Application::unblockInputs();
    };

    animate(newContent, oldContent, anim, done);
}

void pop(Anim anim) {
    auto stack = brls::Application::getActivitiesStack();
    if (anim == Anim::NONE || stack.size() < 2) {
        brls::Application::popActivity(brls::TransitionAnimation::NONE);
        return;
    }

    auto* topActivity = stack.back();
    auto* topContent = topActivity->getContentView();
    auto* underContent = stack[stack.size() - 2]->getContentView();
    topActivity->setInFadeAnimation(true);
    brls::Application::blockInputs();

    auto done = [topActivity] {
        topActivity->setInFadeAnimation(false);
        brls::Application::popActivity(brls::TransitionAnimation::NONE);
        brls::Application::unblockInputs();
    };

    animate(underContent, topContent, anim, done);
}

namespace {

void popNChain(int n, pageNav::Anim anim, std::shared_ptr<std::vector<brls::Activity*>> deferred) {
    if (n == 1) {
        pageNav::pop(anim);
        for (auto* a : *deferred) delete a;
        return;
    }
    auto stack = brls::Application::getActivitiesStack();
    deferred->push_back(stack.back());
    brls::Application::popActivity(brls::TransitionAnimation::NONE, [n, anim, deferred] { popNChain(n - 1, anim, deferred); }, false);
}

} // anonymous namespace

void popN(int n, Anim anim) {
    if (n <= 0) return;
    if (n == 1) { 
        pop(anim); 
        return; 
    }
    auto deferred = std::make_shared<std::vector<brls::Activity*>>();
    popNChain(n, anim, deferred);
}

} // namespace pageNav
