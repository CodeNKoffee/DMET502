# Airport Rush: Cluj-Napoca Last-Minute Boarding

A 2D OpenGL game where you rush through Cluj-Napoca International Airport to catch your flight to Munich (Gate A01) while collecting your friend and avoiding security guards.

## Game Features

### Theme
- **Realistic Setting**: Based on actual Cluj-Napoca International Airport layout
- **Your Story**: Recreate the real experience of rushing to catch a flight with your friend
- **Gate A01**: Your target is the actual gate you used (Domestic gate to Munich)

### Gameplay
- **Player**: Traveler with luggage (you) - starts at security checkpoint
- **Objective**: Reach Gate A01 with your friend before time runs out
- **Collectibles**: Boarding passes (+5 points each)
- **Special Collectible**: Your friend (MUST collect to win!)
- **Obstacles**: Security guards (lose 1 life when hit)
- **Power-ups**: 
  - Manager Approval Badge (5s invincibility)
  - Fast Track Pass (5s speed boost)

### Graphics Requirements Met
- **Player**: 4+ primitives (GL_POLYGON, GL_TRIANGLES, GL_LINE_LOOP, GL_POINTS)
- **Guards**: 2+ primitives (GL_QUADS, GL_TRIANGLES)
- **Collectibles**: 3+ primitives (GL_QUADS, GL_LINES, GL_POINTS)
- **Power-ups**: 2+ primitives each (GL_POLYGON/GL_QUADS, GL_LINE_LOOP/GL_TRIANGLES)
- **Health**: 2+ primitives (GL_QUADS, GL_POLYGON)
- **Panels**: 1+ primitive each (GL_QUADS)

### Animations
- **Player**: Rotates to face movement direction
- **Plane**: Bezier curve horizontal movement
- **Collectibles**: Continuous rotation
- **Power-ups**: Scaling animation
- **Background**: Moving conveyor belt

### Textures (Bonus)
- BMP texture loading implemented
- All game elements have textures applied
- Programmatic texture generation for solid colors

## Controls

### Setup Phase
- **Mouse**: Click objects in bottom panel to select drawing mode
- **Mouse**: Click in game area to place objects
- **R Key**: Start the game

### Game Phase
- **WASD** or **Arrow Keys**: Move player
- **R Key**: Start game (if in setup mode)

## How to Play

1. **Setup**: Click on objects in the bottom panel (Guard, Pass, Badge, Fast) to select them
2. **Place Objects**: Click in the game area to place obstacles, collectibles, and power-ups
3. **Start Game**: Press 'R' to begin the countdown timer
4. **Collect Friend**: Find and collect your friend (special collectible)
5. **Avoid Guards**: Don't hit security guards or you'll lose lives
6. **Use Power-ups**: Collect badges for invincibility and fast track passes for speed
7. **Reach Gate**: Get to Gate A01 with your friend to win!

## Win/Lose Conditions

### Win
- Reach Gate A01 (plane) with your friend collected
- Time remaining > 0
- Lives remaining > 0

### Lose
- Time runs out, OR
- All 5 lives are lost

## Technical Details

### Compilation
```bash
clang++ -o airport_rush P58-SEC-1234.cpp -framework OpenGL -framework GLUT -lGLEW -I/opt/homebrew/include -L/opt/homebrew/lib
```

### Dependencies
- OpenGL
- GLUT (FreeGLUT)
- GLEW
- macOS frameworks

### File Structure
- `P58-SEC-1234.cpp`: Main game file (single file submission)
- `.vscode/`: VSCode configuration for building
- `README.md`: This file

## Game Requirements Checklist

✅ **Environment**
- Top panel with health, score, time
- Bottom panel with object templates
- Game area between panels
- Mouse placement system
- R key to start game

✅ **Player**
- WASD/Arrow key movement
- Boundary checking
- Rotation to face direction
- Collision with obstacles (lose life)
- Collision with collectibles (gain score)
- Collision with power-ups (activate effects)

✅ **Game Target**
- Gate A01 (plane) at opposite position
- Bezier curve animation
- Win condition when reached with friend

✅ **Obstacles**
- Security guards
- Mouse placement system
- Collision detection
- No overlap placement

✅ **Collectibles**
- Boarding passes (+5 points)
- Special friend collectible (required to win)
- Rotation animation
- Mouse placement system

✅ **Power-ups**
- Manager Approval (invincibility)
- Fast Track Pass (speed boost)
- Temporary effects (5 seconds)
- Scaling animation
- Mouse placement system

✅ **Health System**
- 5 lives (stress indicators)
- Non-numerical display
- Updates when life lost

✅ **Animations**
- Player rotation
- Plane Bezier movement
- Collectible rotation
- Power-up scaling
- Background conveyor belt

✅ **Game Logic**
- 60-second timer
- Win/lose conditions
- Score tracking
- State management

✅ **Textures (Bonus)**
- BMP loading function
- Applied to all game elements
- Programmatic texture generation

## Real Airport Layout

The game is based on the actual Cluj-Napoca International Airport:
- **Bottom**: Security/Passport Control (starting area)
- **Middle**: Main terminal corridor
- **Top-left**: Domestic gates A1-A3 (your target is A01)
- **Top-right**: International gates B1-B6

This recreates your real experience of rushing from security to Gate A01 to catch your flight to Munich!
