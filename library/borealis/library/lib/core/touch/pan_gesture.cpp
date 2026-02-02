/*
    Copyright 2021 XITRIX

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <borealis.hpp>

// Delta from touch starting point to current, when
// touch will be recognized as pan movement
#define MAX_DELTA_MOVEMENT 6

// Touch history limit which uses to calculate current pan speed
#define HISTORY_LIMIT 5

// Negative acceleration to calculate
// time to play acceleration animation
#define PAN_SCROLL_ACCELERATION -3000

namespace brls
{

PanGestureRecognizer::PanGestureRecognizer(PanGestureEvent::Callback respond, PanAxis axis)
    : axis(axis)
{
    panEvent.subscribe(respond);
}

GestureState PanGestureRecognizer::recognitionLoop(TouchState touch, MouseState mouse, View* view, Sound* soundToPlay)
{
    if (!enabled)
        return GestureState::FAILED;

    TouchPhase phase = touch.phase;
    Point position   = touch.position;
    int fingerId     = touch.fingerId;

    if (phase == TouchPhase::NONE)
    {
        fingerId = 0;
        position = mouse.position;
        phase    = mouse.leftButton;
    }

    // If not first touch frame and state is
    // INTERRUPTED or FAILED, stop recognition
    if (phase != TouchPhase::START)
    {
        if (this->state == GestureState::INTERRUPTED || this->state == GestureState::FAILED)
        {
            if (this->state != lastState)
                this->panEvent.fire(getCurrentStatus(), soundToPlay);

            lastState = this->state;
            return this->state;
        }
    }

    static PanAcceleration acceleration;
    switch (phase)
    {
        case TouchPhase::START:
            this->posHistory.clear();
            this->state         = GestureState::UNSURE;
            this->startPosition = position;
            this->position      = position;
            this->lastFingerId  = fingerId;
            this->panEvent.fire(getCurrentStatus(), soundToPlay);
            break;
        case TouchPhase::STAY:
        case TouchPhase::END:
            if (lastFingerId != fingerId)
            {
                this->state = GestureState::FAILED;
                lastState   = this->state;
                return this->state;
            }

            this->delta = this->position - position;

            this->position = position;

            // Check if pass any condition to set state START
            if (this->state == GestureState::UNSURE)
            {
                if (fabs(this->startPosition.x - position.x) > MAX_DELTA_MOVEMENT || fabs(this->startPosition.y - position.y) > MAX_DELTA_MOVEMENT)
                {
                    switch (axis)
                    {
                        case PanAxis::HORIZONTAL:
                            if (fabs(delta.x) > fabs(delta.y)) {
                                this->delta = Point();
                                this->startPosition = position;
                                this->state = GestureState::START;
                            }
                            break;
                        case PanAxis::VERTICAL:
                            if (fabs(delta.x) < fabs(delta.y)) {
                                this->delta = Point();
                                this->startPosition = position;
                                this->state = GestureState::START;
                            }
                            break;
                        case PanAxis::ANY:
                            this->delta = Point();
                            this->startPosition = position;
                            this->state = GestureState::START;
                            break;
                    }
                }
            }
            else
            {
                if (phase == TouchPhase::STAY)
                    this->state = GestureState::STAY;
                else
                    this->state = GestureState::END;
            }

            // If last touch frame, calculate acceleration

            if (this->state == GestureState::END)
            {
                float time = posHistory.size() * 1.0f / Application::getFPS();

                float distanceX = posHistory[posHistory.size()-1].x - posHistory[0].x;
                float distanceY = posHistory[posHistory.size()-1].y - posHistory[0].y;

                float velocityX = distanceX / time;
                float velocityY = distanceY / time;
                if (panFactor > 0.0f) {
                    velocityX *= panFactor;
                    velocityY *= panFactor;
                }

                acceleration.time.x = -fabs(velocityX) / PAN_SCROLL_ACCELERATION;
                acceleration.time.y = -fabs(velocityY) / PAN_SCROLL_ACCELERATION;

                acceleration.distance.x = velocityX * acceleration.time.x / 2;
                acceleration.distance.y = velocityY * acceleration.time.y / 2;
            }

            if (this->state == GestureState::START || this->state == GestureState::STAY || this->state == GestureState::END)
            {
                PanGestureStatus state = getCurrentStatus();
                state.acceleration     = acceleration;
                this->panEvent.fire(state, soundToPlay);
            }

            break;
        case TouchPhase::NONE:
            this->state = GestureState::FAILED;
            break;
    }

    // Add current state to history
    posHistory.insert(posHistory.begin(), this->position);
    while (posHistory.size() > HISTORY_LIMIT)
    {
        posHistory.pop_back();
    }

    lastState = this->state;
    return this->state;
}

PanGestureStatus PanGestureRecognizer::getCurrentStatus()
{
    return PanGestureStatus {
        .state         = this->state,
        .position      = this->position,
        .startPosition = this->startPosition,
        .delta         = this->delta,
    };
}

};
