#pragma once

#include <list>
#include <memory>
#include <stdexcept>

#include "IEventHandler.hpp"
#include "SnakeInterface.hpp"

class Event;
class IPort;

namespace Snake
{
struct ConfigurationError : std::logic_error
{
    ConfigurationError();
};

struct UnexpectedEventException : std::runtime_error
{
    UnexpectedEventException();
};

class Controller : public IEventHandler
{
public:
    Controller(IPort& p_displayPort, IPort& p_foodPort, IPort& p_scorePort, std::string const& p_config);

    Controller(Controller const& p_rhs) = delete;
    Controller& operator=(Controller const& p_rhs) = delete;

    void receive(std::unique_ptr<Event> e) override;

    Direction checkDirection(char input);
    Direction adjustDirection(Direction direction);

    void placeNewReceivedFood(bool requestedFoodCollidedWithSnake, Snake::FoodInd foodToPlace);
    void placeNewRequestedFood(bool requestedFoodCollidedWithSnake, Snake::FoodResp foodToPlace);

    bool checkIfNewHeadCollidesWithBody(Segment head);
    bool checkIfReceivedFoodCollidedWithSnake(Snake::FoodInd food);
    bool checkIfRequestedFoodCollidedWithSnake(Snake::FoodResp food);
    
    bool checkIfSnakeCanBeMoved(Segment head);
    void moveSnake(Segment head);

    Segment makeHead();

private:
    

    IPort& m_displayPort;
    IPort& m_foodPort;
    IPort& m_scorePort;

    std::pair<int, int> m_mapDimension;
    std::pair<int, int> m_foodPosition;

    Direction m_currentDirection;
    std::list<Segment> m_segments;
};

} // namespace Snake
