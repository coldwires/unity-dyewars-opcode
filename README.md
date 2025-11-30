# \# DyeWars Project

# 

# A multiplayer grid-based game with a C++ authoritative server and Unity thin client.

# 

# \## Architecture Overview

# 

# ```

# ┌─────────────────────┐         TCP/IP          ┌─────────────────────┐

# │   Unity Client      │ ◄─────────────────────► │   C++ Server        │

# │   (Renderer Only)   │    Custom Protocol      │   (All Game Logic)  │

# └─────────────────────┘                         └─────────────────────┘

# ```

# 

# \*\*Key Design Decision:\*\* The server is \*authoritative\*—it owns all game state. The Unity client is a "dumb terminal" that:

# 1\. Sends player input (key presses)

# 2\. Receives positions to render

# 3\. Does client-side prediction for responsiveness

# 

# ---

# 

# \## Server Architecture (C++)

# 

# \### Threading Model

# 

# ```

# ┌────────────────────────────────────────────────────────────────────┐

# │                         MAIN THREAD                                │

# │  ┌──────────────────────────────────────────────────────────────┐  │

# │  │  asio::io\_context.run()                                      │  │

# │  │  - Accepts new connections                                   │  │

# │  │  - Reads incoming packets (async)                            │  │

# │  │  - Writes outgoing packets (async)                           │  │

# │  └──────────────────────────────────────────────────────────────┘  │

# └────────────────────────────────────────────────────────────────────┘

# 

# ┌────────────────────────────────────────────────────────────────────┐

# │                       GAME LOOP THREAD                             │

# │  ┌──────────────────────────────────────────────────────────────┐  │

# │  │  RunGameLoop() - 20 ticks/second                             │  │

# │  │  - Checks dirty flags                                        │  │

# │  │  - Batches player updates                                    │  │

# │  │  - Broadcasts to all clients                                 │  │

# │  └──────────────────────────────────────────────────────────────┘  │

# └────────────────────────────────────────────────────────────────────┘

# 

# ┌────────────────────────────────────────────────────────────────────┐

# │                       CONSOLE THREAD                               │

# │  ┌──────────────────────────────────────────────────────────────┐  │

# │  │  Reads stdin for commands ('r' = reload, 'q' = quit)         │  │

# │  └──────────────────────────────────────────────────────────────┘  │

# └────────────────────────────────────────────────────────────────────┘

# 

# ┌────────────────────────────────────────────────────────────────────┐

# │                     FILE WATCHER THREAD                            │

# │  ┌──────────────────────────────────────────────────────────────┐  │

# │  │  Monitors Lua scripts for hot-reloading                      │  │

# │  └──────────────────────────────────────────────────────────────┘  │

# └────────────────────────────────────────────────────────────────────┘

# ```

# 

# \### Connection Flow (What happens when a client connects)

# 

# ```cpp

# // 1. GameServer::StartAccept() is listening

# acceptor\_.async\_accept(\[this](std::error\_code ec, asio::ip::tcp::socket socket) {

# &nbsp;   // 2. New connection arrives, assign unique ID

# &nbsp;   uint32\_t id = next\_player\_id\_++;

# &nbsp;   

# &nbsp;   // 3. Create a GameSession to manage this connection

# &nbsp;   auto session = std::make\_shared<GameSession>(std::move(socket), lua\_engine\_, this, id);

# &nbsp;   

# &nbsp;   // 4. Store in sessions map (protected by mutex)

# &nbsp;   {

# &nbsp;       std::lock\_guard<std::mutex> lock(sessions\_mutex\_);

# &nbsp;       sessions\_\[id] = session;

# &nbsp;   }

# &nbsp;   

# &nbsp;   // 5. Start the session (sends initial data, begins reading)

# &nbsp;   session->Start();

# &nbsp;   

# &nbsp;   // 6. Keep listening for more connections

# &nbsp;   StartAccept();

# });

# ```

# 

# ```cpp

# // Inside GameSession::Start()

# void GameSession::Start() {

# &nbsp;   SendPlayerID();           // Tell client "you are player #X"

# &nbsp;   SendPosition();           // Tell client "you are at (0,0)"

# &nbsp;   SendAllPlayers();         // Tell client about all other players

# &nbsp;   BroadcastPlayerJoined();  // Tell OTHER players "player #X joined"

# &nbsp;   ReadPacketHeader();       // Begin async read loop

# }

# ```

# 

# \### Packet Protocol

# 

# All packets follow this structure:

# 

# ```

# ┌─────────┬─────────┬─────────┬─────────┬─────────────────────┐

# │  0x11   │  0x68   │ size\_hi │ size\_lo │      payload        │

# │ (magic) │ (magic) │         │         │   (variable len)    │

# └─────────┴─────────┴─────────┴─────────┴─────────────────────┘

# &nbsp;  byte 0    byte 1    byte 2    byte 3      bytes 4+

# ```

# 

# \*\*Why the magic header `0x11 0x68`?\*\*

# \- Validates packet boundaries (TCP is a stream, not message-based)

# \- Detects corrupted/malformed data

# \- Helps identify garbage or malicious packets

# 

# \### Message Types (Opcodes)

# 

# | Opcode | Direction | Name | Payload |

# |--------|-----------|------|---------|

# | `0x01` | Client→Server | Move | `\[direction]` (0=up, 1=right, 2=down, 3=left) |

# | `0x02` | Client→Server | Request Position | (empty) |

# | `0x03` | Client→Server | Custom Message | `\[data...]` |

# | `0x10` | Server→Client | Your Position | `\[x]\[y]` |

# | `0x12` | Server→Client | Other Player Update | `\[id:4bytes]\[x]\[y]` |

# | `0x13` | Server→Client | Your Player ID | `\[id:4bytes]` |

# | `0x14` | Server→Client | Player Left | `\[id:4bytes]` |

# | `0x20` | Server→Client | Batch Update | `\[count]\[id,x,y]\[id,x,y]...` |

# 

# \### The Dirty Flag Pattern

# 

# \*\*Problem:\*\* If we broadcast every player's position immediately when they move, with 100 players moving 10 times/second, that's 1000 broadcasts × 100 recipients = 100,000 packets/second.

# 

# \*\*Solution:\*\* Batch updates using a "dirty flag":

# 

# ```cpp

# // When player moves (in HandlePacket)

# bool moved = player\_->AttemptMove(direction, server\_->GetMap());

# if (moved) {

# &nbsp;   is\_dirty\_ = true;  // Mark as "needs to be broadcast"

# &nbsp;   SendPosition();    // Only tell THIS player immediately

# }

# 

# // In game loop (20 times/second)

# void GameServer::ProcessUpdates() {

# &nbsp;   std::vector<PlayerData> moving\_players;

# &nbsp;   

# &nbsp;   for (auto\& pair : sessions\_) {

# &nbsp;       if (pair.second->IsDirty()) {

# &nbsp;           moving\_players.push\_back(pair.second->GetPlayerData());

# &nbsp;           pair.second->SetDirty(false);  // Reset flag

# &nbsp;       }

# &nbsp;   }

# &nbsp;   

# &nbsp;   // Build ONE packet with ALL movements

# &nbsp;   // Send that ONE packet to ALL clients

# &nbsp;   // Result: 20 packets/second instead of 100,000

# }

# ```

# 

# \*\*Why `std::atomic<bool>`?\*\*

# \- `is\_dirty\_` is written by network thread (when packet arrives)

# \- `is\_dirty\_` is read by game loop thread (every tick)

# \- Without atomic, you get a data race (undefined behavior)

# 

# \### Class Responsibilities

# 

# | Class | Responsibility |

# |-------|----------------|

# | `GameServer` | Accepts connections, owns all sessions, runs game loop, broadcasts |

# | `GameSession` | Manages one client connection, handles packets, owns one Player |

# | `Player` | Stores position, handles movement logic with collision |

# | `GameMap` | Stores walkable/wall data, answers "is this tile walkable?" |

# | `LuaGameEngine` | Runs Lua scripts for game logic, supports hot-reload |

# | `DatabaseManager` | SQLite wrapper for player accounts (not yet integrated) |

# 

# ---

# 

# \## Client Architecture (Unity/C#)

# 

# \### Threading Model

# 

# ```

# ┌────────────────────────────────────────────────────────────────────┐

# │                         MAIN THREAD                                │

# │  - Unity Update() loop                                             │

# │  - Handles input                                                   │

# │  - Renders game objects                                            │

# │  - Processes queued actions from network thread                    │

# └────────────────────────────────────────────────────────────────────┘

# 

# ┌────────────────────────────────────────────────────────────────────┐

# │                       RECEIVE THREAD                               │

# │  - Blocking read on TCP socket                                     │

# │  - Parses packets                                                  │

# │  - Queues actions to main thread (can't touch Unity from here!)   │

# └────────────────────────────────────────────────────────────────────┘

# ```

# 

# \### Thread-Safe Communication Pattern

# 

# Unity APIs can only be called from the main thread. The receive thread queues work:

# 

# ```csharp

# // Network thread receives data, queues an action

# lock (queueLock) {

# &nbsp;   mainThreadActions.Enqueue(() => {

# &nbsp;       // This code runs on main thread in Update()

# &nbsp;       MyPosition = new Vector2Int(x, y);

# &nbsp;       OnMyPositionUpdated?.Invoke(MyPosition);

# &nbsp;   });

# }

# 

# // Main thread processes queue every frame

# void Update() {

# &nbsp;   lock (queueLock) {

# &nbsp;       while (mainThreadActions.Count > 0) {

# &nbsp;           mainThreadActions.Dequeue()?.Invoke();

# &nbsp;       }

# &nbsp;   }

# }

# ```

# 

# \### Client-Side Prediction

# 

# To make movement feel responsive (no waiting for server round-trip):

# 

# ```csharp

# public void SendMoveCommand(int direction) {

# &nbsp;   // 1. Send to server

# &nbsp;   byte\[] message = new byte\[] { 0x01, (byte)direction };

# &nbsp;   SendMessage(message);

# &nbsp;   

# &nbsp;   // 2. Immediately predict the result locally

# &nbsp;   Vector2Int predictedPos = MyPosition;

# &nbsp;   if (direction == 0) predictedPos.y += 1;      // UP

# &nbsp;   else if (direction == 1) predictedPos.x += 1; // RIGHT

# &nbsp;   // ...

# &nbsp;   

# &nbsp;   // 3. Update local position (renders immediately)

# &nbsp;   MyPosition = predictedPos;

# &nbsp;   OnMyPositionUpdated?.Invoke(MyPosition);

# }

# 

# // Later, when server confirms (or corrects):

# case 0x10: // Server says "you are at (x,y)"

# &nbsp;   if (MyPosition.x != x || MyPosition.y != y) {

# &nbsp;       // Server disagrees! Snap to correct position.

# &nbsp;       Debug.Log($"Correction: Snapped to ({x}, {y})");

# &nbsp;       MyPosition = new Vector2Int(x, y);

# &nbsp;   }

# &nbsp;   break;

# ```

# 

# ---

# 

# \## Lua Scripting System

# 

# Game logic can be modified without recompiling the server:

# 

# ```lua

# -- scripts/main.lua

# 

# function process\_move\_command(x, y, direction)

# &nbsp;   local new\_x, new\_y = x, y

# &nbsp;   

# &nbsp;   -- Basic movement

# &nbsp;   if direction == 0 and y > 0 then new\_y = y - 1 end

# &nbsp;   -- ...

# &nbsp;   

# &nbsp;   -- Custom game rules (walls, teleports, traps)

# &nbsp;   if new\_x == 5 and new\_y == 5 then

# &nbsp;       log("Blocked by wall!")

# &nbsp;       return {x, y}  -- Return original position

# &nbsp;   end

# &nbsp;   

# &nbsp;   return {new\_x, new\_y}

# end

# 

# function on\_player\_moved(player\_id, x, y)

# &nbsp;   -- React to player movement (traps, triggers, etc.)

# end

# ```

# 

# \*\*Hot Reload:\*\* Save the file, type 'r' in server console (or it auto-detects changes).

# 

# ---

# 

# \## Building \& Running

# 

# \### Server (C++ with vcpkg)

# 

# ```bash

# cd dyewars\_server

# mkdir build \&\& cd build

# cmake .. -DCMAKE\_TOOLCHAIN\_FILE=\[vcpkg-root]/scripts/buildsystems/vcpkg.cmake

# cmake --build .

# ./DyeWarsServer

# ```

# 

# \*\*Dependencies (via vcpkg):\*\*

# \- asio

# \- nlohmann\_json

# \- spdlog

# \- sol2

# \- lua

# \- sqlite3

# 

# \### Client (Unity)

# 

# 1\. Open `dyewars/` folder in Unity

# 2\. Ensure `NetworkManager` has correct server IP (currently `192.168.1.3`)

# 3\. Press Play

# 

# ---

# 

# \## Project Structure

# 

# ```

# DyeWarsProject/

# ├── dyewars/                    # Unity client

# │   └── Assets/

# │       └── code/

# │           ├── NetworkManager.cs    # Networking, input, protocol

# │           └── GridRenderer.cs      # Renders players on grid

# │

# ├── dyewars\_server/             # C++ server

# │   ├── include/

# │   │   ├── server/

# │   │   │   ├── GameServer.h    # Main server class

# │   │   │   ├── GameSession.h   # Per-client connection handler

# │   │   │   ├── Player.h        # Player state and movement

# │   │   │   ├── GameMap.h       # Tile/collision data

# │   │   │   └── Common.h        # Packet struct, shared types

# │   │   ├── lua/

# │   │   │   └── LuaEngine.h     # Lua scripting interface

# │   │   └── database/

# │   │       └── DatabaseManager.h  # SQLite wrapper

# │   ├── src/

# │   │   ├── main.cpp            # Entry point

# │   │   ├── server/             # Implementation files

# │   │   └── lua/

# │   └── scripts/

# │       └── main.lua            # Hot-reloadable game logic

# │

# └── README.md

# ```

# 

# ---

# 

# \## Learning Notes

# 

# This project demonstrates several important game networking concepts:

# 

# 1\. \*\*Authoritative Server\*\* - Client never decides game state, only sends input

# 2\. \*\*Client-Side Prediction\*\* - Move locally, correct if server disagrees

# 3\. \*\*Packet Coalescing\*\* - Batch many updates into one packet

# 4\. \*\*Dirty Flag Pattern\*\* - Track what changed, only sync changes

# 5\. \*\*Thread Safety\*\* - Mutexes for shared data, atomics for flags

# 6\. \*\*Hot Reloading\*\* - Change game logic without restarting server

# 7\. \*\*Async I/O\*\* - Non-blocking network operations with Boost.Asio

