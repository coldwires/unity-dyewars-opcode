-- Main game logic script - can be modified while server is running!

function process_move_command(x, y, direction)
    log(string.format("Processing moves: (%d, %d) direction: %d", x, y, direction))

    local new_x, new_y = x, y

    -- Basic boundary checking
    if direction == DIRECTION_UP and y > 0 then
        new_y = y - 1
    elseif direction == DIRECTION_RIGHT and x < GRID_WIDTH - 1 then
        new_x = x + 1
    elseif direction == DIRECTION_DOWN and y < GRID_HEIGHT - 1 then
        new_y = y + 1
    elseif direction == DIRECTION_LEFT and x > 0 then
        new_x = x - 1
    end

    -- Add custom game rules here!
    -- Example: Create a wall at (5,5)
    if new_x == 5 and new_y == 5 then
        log("Blocked by wall at (5,5)!")
        return {x, y} -- Return original position
    end

    -- Example: Teleport at (9,9)
    if new_x == 9 and new_y == 9 then
        log("Teleporting to (0,0)!")
        return {0, 0}
    end

    -- Example: Ice slide - keep moving in same direction
    if new_x == 7 and new_y == 7 then
        log("Ice slide!")
        if direction == DIRECTION_RIGHT and new_x < GRID_WIDTH - 1 then
            new_x = new_x + 1
        elseif direction == DIRECTION_LEFT and new_x > 0 then
            new_x = new_x - 1
        end
    end

    return {new_x, new_y}
end

function process_custom_message(data)
    log("Processing custom message with " .. #data .. " bytes")

    -- Echo back with modification as example
    local response = {}
    for i = 1, #data do
        response[i] = data[i] + 1 -- Simple transformation
    end

    return response
end

function on_player_moved(player_id, x, y, dir)
    if x == 5 and y == 1 then
        log("Player" .. player_id .. "stepped on a trap! Facing: " .. dir)
    end
end

-- Global game state that persists across reloads (in this session)
game_state = game_state or {
    player_count = 0,
    total_moves = 0
}

function get_game_stats()
    return {
        players = game_state.player_count,
        total_moves = game_state.total_moves
    }
end

log("Game logic script loaded successfully!")
log("Try modifying this file and typing 'reload' in the server console!")