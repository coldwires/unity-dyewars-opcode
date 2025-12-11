
function process_move_command(x, y, direction)
    log(string.format("Move: (%d,%d) dir:%d", x, y, direction))
    local new_x, new_y = x, y
    if direction == DIRECTION_UP and y > 0 then new_y = y - 1
    elseif direction == DIRECTION_RIGHT and x < GRID_WIDTH - 1 then new_x = x + 1
    elseif direction == DIRECTION_DOWN and y < GRID_HEIGHT - 1 then new_y = y + 1
    elseif direction == DIRECTION_LEFT and x > 0 then new_x = x - 1 end
    return {new_x, new_y}
end
function process_custom_message(data) return data end
log("Game script loaded!")
