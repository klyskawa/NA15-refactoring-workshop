#include "SnakeController.hpp"

#include <algorithm>
#include <sstream>

#include "EventT.hpp"
#include "IPort.hpp"

namespace Snake
{
ConfigurationError::ConfigurationError()
    : std::logic_error("Bad configuration of Snake::Controller.")
{}

UnexpectedEventException::UnexpectedEventException()
    : std::runtime_error("Unexpected event received!")
{}

Direction Controller::checkDirection(char input)
{
    switch (input) {
            case 'U':
                return Direction_UP;
                break;
            case 'D':
                return Direction_DOWN;
                break;
            case 'L':
                return Direction_LEFT;
                break;
            case 'R':
                return Direction_RIGHT;
                break;
            default:
                throw ConfigurationError();
        }
}

Direction Controller::adjustDirection(Direction direction)
{
    if ((m_currentDirection & 0b01) != (direction & 0b01)) {
        return direction;
    }
}

void Controller::placeNewReceivedFood(bool requestedFoodCollidedWithSnake, Snake::FoodInd foodToPlace)
{
    if (requestedFoodCollidedWithSnake) {
        m_foodPort.send(std::make_unique<EventT<FoodReq>>());
    } else {
        
        DisplayInd clearOldFood;
        clearOldFood.x = m_foodPosition.first;
        clearOldFood.y = m_foodPosition.second;
        clearOldFood.value = Cell_FREE;
        m_displayPort.send(std::make_unique<EventT<DisplayInd>>(clearOldFood));
        
        DisplayInd placeNewFood;
        placeNewFood.x = foodToPlace.x;
        placeNewFood.y = foodToPlace.y;
        placeNewFood.value = Cell_FOOD;
        m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewFood));
    }
}

void Controller::placeNewRequestedFood(bool requestedFoodCollidedWithSnake, Snake::FoodResp foodToPlace)
{
    if (requestedFoodCollidedWithSnake) {
        m_foodPort.send(std::make_unique<EventT<FoodReq>>());
    } else {
        DisplayInd placeNewFood;
        placeNewFood.x = foodToPlace.x;
        placeNewFood.y = foodToPlace.y;
        placeNewFood.value = Cell_FOOD;
        m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewFood));
    }
}

bool Controller::checkIfNewHeadCollidesWithBody(Segment head)
{
    for (auto segment : m_segments) {
        if (segment.x == head.x and segment.y == head.y) {
            m_scorePort.send(std::make_unique<EventT<LooseInd>>());
            return true;
        }
    }
    return false;
}

bool Controller::checkIfReceivedFoodCollidedWithSnake(Snake::FoodInd food)
{
    for (auto const& segment : m_segments) {
        if (segment.x == food.x and segment.y == food.y) {
            return true;
        }
    }
    return false;
}

bool Controller::checkIfRequestedFoodCollidedWithSnake(Snake::FoodResp food)
{
    for (auto const& segment : m_segments) {
        if (segment.x == food.x and segment.y == food.y) {
            return true;
        }
    }
    return false;
}

Segment Controller::makeHead()
{
    Segment const& currentHead = m_segments.front();
    Segment newHead;

    newHead.x = currentHead.x + ((m_currentDirection & 0b01) ? (m_currentDirection & 0b10) ? 1 : -1 : 0);
    newHead.y = currentHead.y + (not (m_currentDirection & 0b01) ? (m_currentDirection & 0b10) ? 1 : -1 : 0);
    newHead.ttl = currentHead.ttl;

    return newHead;
}

bool Controller::checkIfSnakeCanBeMoved(Segment head)
{
    if (std::make_pair(head.x, head.y) == m_foodPosition)
    {
        m_scorePort.send(std::make_unique<EventT<ScoreInd>>());
        m_foodPort.send(std::make_unique<EventT<FoodReq>>());
    }
    else if (head.x < 0 or head.y < 0 or
             head.x >= m_mapDimension.first or
             head.y >= m_mapDimension.second)
    {
        m_scorePort.send(std::make_unique<EventT<LooseInd>>());
        return true;
    }
    else
    {
        for (auto &segment : m_segments)
        {
            if (not --segment.ttl)
            {
                DisplayInd l_evt;
                l_evt.x = segment.x;
                l_evt.y = segment.y;
                l_evt.value = Cell_FREE;

                m_displayPort.send(std::make_unique<EventT<DisplayInd>>(l_evt));
            }
        }
    }
    return false;
}

void Controller::moveSnake(Segment head)
{
    m_segments.push_front(head);
    DisplayInd placehead;
    placehead.x = head.x;
    placehead.y = head.y;
    placehead.value = Cell_SNAKE;

    m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placehead));

    m_segments.erase(
        std::remove_if(
            m_segments.begin(),
            m_segments.end(),
            [](auto const &segment)
            { return not(segment.ttl > 0); }),
        m_segments.end());
}

Controller::Controller(IPort& p_displayPort, IPort& p_foodPort, IPort& p_scorePort, std::string const& p_config)
    : m_displayPort(p_displayPort),
      m_foodPort(p_foodPort),
      m_scorePort(p_scorePort)
{
    std::istringstream istr(p_config);
    char w, f, s, d;

    int width, height, length;
    int foodX, foodY;
    istr >> w >> width >> height >> f >> foodX >> foodY >> s;

    if (w == 'W' and f == 'F' and s == 'S') {
        m_mapDimension = std::make_pair(width, height);
        m_foodPosition = std::make_pair(foodX, foodY);

        istr >> d;
        
        m_currentDirection = checkDirection(d);
        istr >> length;

        while (length) {
            Segment seg;
            istr >> seg.x >> seg.y;
            seg.ttl = length--;

            m_segments.push_back(seg);
        }
    } else {
        throw ConfigurationError();
    }
}

void Controller::receive(std::unique_ptr<Event> e)
{
    try {
        auto const& timerEvent = *dynamic_cast<EventT<TimeoutInd> const&>(*e);

        Segment newHead = makeHead();

        bool lost = checkIfNewHeadCollidesWithBody(newHead);

        if (not lost)
            lost = checkIfSnakeCanBeMoved(newHead);

        if (not lost) {
            moveSnake(newHead);
        }
    } catch (std::bad_cast&) {
        try {
            auto direction = dynamic_cast<EventT<DirectionInd> const&>(*e)->direction;

            m_currentDirection = adjustDirection(direction);
            
        } catch (std::bad_cast&) {
            try {
                auto receivedFood = *dynamic_cast<EventT<FoodInd> const&>(*e);

                bool requestedFoodCollidedWithSnake = checkIfReceivedFoodCollidedWithSnake(receivedFood);
                
                placeNewReceivedFood(requestedFoodCollidedWithSnake, receivedFood);

                m_foodPosition = std::make_pair(receivedFood.x, receivedFood.y);

            } catch (std::bad_cast&) {
                try {
                    auto requestedFood = *dynamic_cast<EventT<FoodResp> const&>(*e);

                    bool requestedFoodCollidedWithSnake = checkIfRequestedFoodCollidedWithSnake(requestedFood);

                    placeNewRequestedFood(requestedFoodCollidedWithSnake, requestedFood);

                    m_foodPosition = std::make_pair(requestedFood.x, requestedFood.y);
                } catch (std::bad_cast&) {
                    throw UnexpectedEventException();
                }
            }
        }
    }
}

} // namespace Snake
