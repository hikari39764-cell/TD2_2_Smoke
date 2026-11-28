#include "BurnFieldRules.h"

namespace Project_TD2_2 {

void BurnFieldRules::Initialize(HIKARI::ObjectView object, BurnField& field) {
    if (field.initialized)
        return;
    field.initialized = true;
    HIKARI::GameProjectTransform transform{};
    if (!object.GetWorldTransform(transform))
        return;
    field.map.Reset(transform.position, transform.position.y, field.fieldSize, field.cellSize);
}

} // namespace Project_TD2_2
