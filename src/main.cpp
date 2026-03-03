#include <graphics.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>
#include <vector>

struct Point {
    int x, y;
};

struct Seed {
    double x, y;
    double angle;
    double velocityY;
    double velocityX;
    bool active;
};

class AnimatedTreeDrawer {
   private:
    bool isPaused;
    int screenWidth, screenHeight;
    int groundLevel;
    int seedX, seedY;
    std::chrono::steady_clock::time_point lastTime;
    double sunAngle;

    // Animation state variables
    double treeGrowthScale;
    double flowerScale;
    bool showFlowers;
    int flowerPosX, flowerPosY;
    std::vector<Point> flowerPositions;
    std::vector<Seed> fallingSeeds;
    int animationPhase;  // 0: seed germination, 1: tree growth, 2: flowering, 3: seed fall, 4: reset
    int phaseTimer;
    int rightmostBranchX, rightmostBranchY;
    double zoomScale;
    double cameraOffsetX, cameraOffsetY;

    // Colors
    const double PI = 3.141592654;
    const int BROWN = COLOR(139, 69, 19);
    const int DARK_BROWN = COLOR(101, 67, 33);
    const int LEAF_GREEN = COLOR(34, 139, 34);
    const int LIGHT_GREEN = COLOR(50, 205, 50);
    const int SKY_BLUE = COLOR(135, 206, 235);
    const int SOIL_BROWN = COLOR(90, 50, 20);

    void drawSeed(int x, int y, double angle, double scale = 1.0) {
        int oldColor = getcolor();

        setcolor(COLOR(160, 82, 45));
        setfillstyle(SOLID_FILL, COLOR(160, 82, 45));

        int size = static_cast<int>(8 * scale);

        // Draw seed as rotated oval using rotation transformation
        // Calculate 4 points of the oval and rotate them
        const int numPoints = 12;
        int points[numPoints * 2];

        for (int i = 0; i < numPoints; i++) {
            double t = i * 2 * PI / numPoints;
            // Oval shape (wider than tall)
            double localX = size * cos(t);
            double localY = size * 0.5 * sin(t);

            // Apply rotation transformation
            int rotatedX = static_cast<int>(localX * cos(angle) - localY * sin(angle));
            int rotatedY = static_cast<int>(localX * sin(angle) + localY * cos(angle));

            points[i * 2] = x + rotatedX;
            points[i * 2 + 1] = y + rotatedY;
        }

        // Fill the rotated seed
        fillpoly(numPoints, points);

        // Draw a line through the seed to show rotation clearly
        int lineLength = static_cast<int>(size * 0.8);
        int lineX1 = x + static_cast<int>(lineLength * cos(angle));
        int lineY1 = y + static_cast<int>(lineLength * sin(angle));
        int lineX2 = x - static_cast<int>(lineLength * cos(angle));
        int lineY2 = y - static_cast<int>(lineLength * sin(angle));

        setcolor(COLOR(100, 50, 20));
        setlinestyle(SOLID_LINE, 0, std::max(1, static_cast<int>(scale / 3)));
        line(lineX1, lineY1, lineX2, lineY2);

        setcolor(oldColor);
    }

    void drawSoil() {
        // Lighter, unified brown for the entire underground section
        int soilColor = COLOR(110, 70, 40);
        setfillstyle(SOLID_FILL, soilColor);
        setcolor(soilColor);
        bar(0, groundLevel, screenWidth, screenHeight);
    }

    void drawBranch(int x1, int y1, double length, double angle, int depth, double scale, double growthProgress = 1.0) {
        double currentVisualLength = length * scale;
        if (currentVisualLength < 2.0 || depth <= 0) return;

        // Use Dark Brown for the thick trunk, and lighter brown for branches
        if (depth >= 5) {
            setcolor(DARK_BROWN);
        } else {
            setcolor(BROWN);
        }

        double branchProgress = std::min(1.0, std::max(0.0, (growthProgress * 10) - (8 - depth)));
        if (branchProgress <= 0) return;

        double scaledLength = currentVisualLength * branchProgress;
        int x2 = x1 + static_cast<int>(scaledLength * cos(angle));
        int y2 = y1 - static_cast<int>(scaledLength * sin(angle));

        // Draw the Branch Line
        setlinestyle(SOLID_LINE, 0, std::max(1, static_cast<int>(depth * scale)));
        line(x1, y1, x2, y2);

        // --- LEAF LOGIC with Color Variation ---
        if (depth <= 5 && branchProgress > 0.6) {
            int leafSize = std::max(2, static_cast<int>(5 * scale));
            int numLeaves = 10;  // Increased count for even more density

            for (int i = 0; i < numLeaves; i++) {
                // --- Color Selection ---
                if (i % 3 == 0) {
                    setcolor(LIGHT_GREEN);
                    setfillstyle(SOLID_FILL, LIGHT_GREEN);
                } else if (i % 3 == 1) {
                    setcolor(LEAF_GREEN);
                    setfillstyle(SOLID_FILL, LEAF_GREEN);
                } else {
                    setcolor(COLOR(34, 139, 34));  // Forest Green
                    setfillstyle(SOLID_FILL, COLOR(34, 139, 34));
                }

                // 1. STAGGERING: Place some leaves at the tip, and some slightly back
                // 0.9 is 90% of the way along the branch, 1.0 is the very tip.
                double branchPos = (i % 2 == 0) ? 1.0 : 0.85;
                int bx = x1 + (x2 - x1) * branchPos;
                int by = y1 + (y2 - y1) * branchPos;

                // 2. SCATTER: Displace them in a wider, more natural "cloud"
                // We use sin/cos for a circular spread, but vary the radius
                double angle = i * (6.28 / numLeaves);
                int radius = (i % 3 == 0) ? 12 : 6;  // Some leaves stray further out
                radius *= scale;

                int offsetX = cos(angle) * radius;

                // 3. VERTICAL BIAS: Specifically push some up and some down as requested
                // Using (i * 2 - 10) creates a vertical spread from -10 to +10
                int offsetY = (sin(angle) * radius) + (i % 4 == 0 ? -8 : 4);

                fillellipse(bx + offsetX, by + offsetY, leafSize, leafSize + 1);
            }
            setcolor(BROWN);  // Reset for next branch
        }

        // --- GRADUAL FLOWERING LOGIC ---
        if (showFlowers && branchProgress > 0.9) {
            // Tier 1: Deepest flowers start first
            if (depth == 3 && flowerScale > 0.3) {
                // These flowers appear when the scale is at 30%
                double localScale = (flowerScale - 0.3) / 0.7;
                drawFlower(x2, y2, localScale);
            }

            // Tier 2: Middle flowers
            if (depth == 2 && flowerScale > 0.6) {
                // These flowers appear when the scale is at 60%
                double localScale = (flowerScale - 0.6) / 0.4;
                drawFlower(x2, y2, localScale);
            }

            // Tier 3: Tip flowers
            if (depth == 1 && flowerScale > 0.8) {
                // These flowers appear last, at 80% scale
                double localScale = (flowerScale - 0.8) / 0.2;
                drawFlower(x2, y2, localScale);
            }
        }

        double newLength = length * 0.7;
        drawBranch(x2, y2, newLength, angle - 0.45, depth - 1, scale, growthProgress);
        drawBranch(x2, y2, newLength, angle + 0.45, depth - 1, scale, growthProgress);
        drawBranch(x2, y2, newLength * 0.8, angle, depth - 1, scale, growthProgress);
    }

    void drawFlower(int x, int y, double scale) {
        if (scale <= 0) return;

        int oldColor = getcolor();

        // We keep your original base size intact so it doesn't break the tree's proportions
        int baseSize = std::max(2, static_cast<int>(3 * scale));

        // --- COLORS (Canopy / Poppy Style) ---
        // Canopy blossoms are usually creamy white or pale blush
        int shadowPetalColor = COLOR(240, 220, 210);  // Slightly darker for depth
        int frontPetalColor = COLOR(255, 245, 238);   // Creamy Seashell White
        int centerDark = COLOR(139, 0, 0);            // Deep maroon "poppy" center
        int centerLight = YELLOW;                     // Bright pollen dot

        // --- DRAWING THE POPPY SHAPE ---
        // A poppy has 4 broad, overlapping petals forming a cup/cross shape.

        // 1. Back Petals (Vertical-ish) - Drawn slightly darker for a 3D shadow effect
        setcolor(shadowPetalColor);
        setfillstyle(SOLID_FILL, shadowPetalColor);
        fillellipse(x, y - baseSize, baseSize + 1, baseSize + 2);  // Top petal
        fillellipse(x, y + baseSize, baseSize + 1, baseSize + 2);  // Bottom petal

        // 2. Front Petals (Horizontal-ish) - Lighter color, drawn on top
        setcolor(frontPetalColor);
        setfillstyle(SOLID_FILL, frontPetalColor);
        fillellipse(x - baseSize, y, baseSize + 2, baseSize + 1);  // Left petal
        fillellipse(x + baseSize, y, baseSize + 2, baseSize + 1);  // Right petal

        // 3. The Dark Poppy Center
        // Poppies are famous for their high-contrast, dark middle
        setcolor(centerDark);
        setfillstyle(SOLID_FILL, centerDark);
        int centerRadius = std::max(1, (baseSize / 2) + 1);
        fillellipse(x, y, centerRadius, centerRadius);

        // 4. The Pollen Dot (Pistil)
        setcolor(centerLight);
        setfillstyle(SOLID_FILL, centerLight);
        int dotRadius = std::max(1, baseSize / 3);
        fillellipse(x, y, dotRadius, dotRadius);

        // Restore the original color so the branches don't turn white!
        setcolor(oldColor);
    }

    // Draw the sun
    void drawSun() {
        int radius = 30;
        int skyHeight = 150;
        int sunX = static_cast<int>(screenWidth * sunAngle / PI);
        int sunY = static_cast<int>(skyHeight * sin(sunAngle)) + 50;

        setcolor(YELLOW);
        setfillstyle(SOLID_FILL, YELLOW);
        fillellipse(sunX, sunY, radius, radius);

        for (int i = 0; i < 12; i++) {
            double angle = i * 30 * PI / 180.0;
            int x1 = sunX + static_cast<int>((radius + 5) * cos(angle));
            int y1 = sunY + static_cast<int>((radius + 5) * sin(angle));
            int x2 = sunX + static_cast<int>((radius + 20) * cos(angle));
            int y2 = sunY + static_cast<int>((radius + 20) * sin(angle));
            line(x1, y1, x2, y2);
        }
    }

    // Draw clouds
    void drawClouds() {
        setcolor(WHITE);
        setfillstyle(SOLID_FILL, WHITE);

        for (int cloud = 0; cloud < 3; cloud++) {
            int cloudX = 100 + cloud * 200;
            int cloudY = 80 + (cloud * 17) % 50;

            for (int i = 0; i < 5; i++) {
                int circleX = cloudX + i * 25;
                int circleY = cloudY + ((i * 13) % 20 - 10);
                int radius = 20 + (i * 7) % 10;
                fillellipse(circleX, circleY, radius, radius);
            }
        }
    }

    void updateFallingSeeds() {
        for (auto& seed : fallingSeeds) {
            if (!seed.active) continue;

            if (seed.y < groundLevel) {
                // AIR PHASE
                seed.velocityY += 0.02;
                seed.y += seed.velocityY;
                seed.x += seed.velocityX;

                // --- PERFECT ROTATION SYNC ---
                // Calculate how far we are from the ground (0.0 to 1.0)
                // 300 is roughly the drop height.
                double distanceToGround = groundLevel - seed.y;

                // We want the angle to be a multiple of 2*PI (6.28) when distance is 0.
                // We force the angle to "align" as it gets closer.
                // This formula spins fast when high, and aligns perfectly as it hits 0.
                seed.angle = (distanceToGround / 35.0) + (PI * 4);
            } else {
                // GROUND PHASE - HARD LOCK
                // The moment we touch ground, we force Angle to 0.
                // Since our formula above aligns to near-zero at distance 0, this is seamless.
                seed.angle = 0;
                seed.velocityY = 0;
                seed.velocityX = 0;

                // Sinking Logic
                if (seed.y < groundLevel + 25) {
                    seed.y += 0.5;  // Sink
                } else {
                    // STOP SHAKING: Hard set the final position
                    seed.y = groundLevel + 25;
                }
            }
        }
    }

    // Display phase information
    void displayPhaseInfo() {
        setcolor(COLOR(20, 20, 60));  // High-contrast navy
        settextstyle(TRIPLEX_FONT, HORIZ_DIR, 2);

        char title[100];
        switch (animationPhase) {
            case 0:
                sprintf(title, "Phase 1: Seed Germination");
                break;
            case 1:
                sprintf(title, "Phase 2: Seedling Growth");
                break;
            case 2:
                sprintf(title, "Phase 3: Tree Maturation");
                break;
            case 3:
                sprintf(title, "Phase 4: Flowering");
                break;
            case 4:
                sprintf(title, "Phase 5: Seed Dispersal");
                break;
            case 5:
                sprintf(title, "Phase 6: Cycle Reset");
                break;
        }
        outtextxy(20, 20, title);
    }

    void drawPauseMenu() {
        int boxW = 460, boxH = 180;
        int midX = screenWidth / 2;
        int midY = screenHeight / 2;
        int borderColor = COLOR(80, 50, 30);

        // 1. Draw the Menu Box
        setcolor(borderColor);
        setlinestyle(SOLID_LINE, 0, 3);
        rectangle(midX - boxW / 2, midY - boxH / 2, midX + boxW / 2, midY + boxH / 2);

        setfillstyle(SOLID_FILL, WHITE);
        floodfill(midX, midY, borderColor);

        // 2. Text Setup
        setbkcolor(WHITE);  // Removes text background artifacts
        setcolor(BLACK);

        // Header (Fixed aspect ratio)
        settextstyle(TRIPLEX_FONT, HORIZ_DIR, 3);
        outtextxy(midX - 75, midY - 65, (char*)"PAUSED");

        // Instructions
        settextstyle(TRIPLEX_FONT, HORIZ_DIR, 2);
        outtextxy(midX - 160, midY + 10, (char*)"[Space] - Play / Pause");
        outtextxy(midX - 160, midY + 35, (char*)"[R]     - Reset Animation");
        outtextxy(midX - 160, midY + 60, (char*)"[Q]     - Quit Application");
    }

    void drawSeedlingLeaves(int x, int y, double progress) {
        int stemHeight = static_cast<int>(60 * progress);

        setcolor(LEAF_GREEN);
        setlinestyle(SOLID_LINE, 0, std::max(2, static_cast<int>(progress * 4)));
        line(x, y, x, y - stemHeight);

        if (progress > 0.2) {
            double leafProgress = (progress - 0.2) / 0.8;
            int leafSize = static_cast<int>(20 * leafProgress);
            int leafYOffset = static_cast<int>(stemHeight * 0.5);

            setcolor(LIGHT_GREEN);
            setfillstyle(SOLID_FILL, LIGHT_GREEN);
            // Left and Right leaves
            fillellipse(x - leafSize, y - leafYOffset, leafSize, static_cast<int>(leafSize * 0.6));
            fillellipse(x + leafSize, y - leafYOffset, leafSize, static_cast<int>(leafSize * 0.6));
        }
    }

    void drawRoot(int x1, int y1, double length, double angle, int depth, double growthProgress, int surfaceLimitY) {
        if (depth <= 0 || growthProgress <= 0) return;

        // Earthy brown colors
        int rootColor = (depth > 3) ? COLOR(140, 100, 60) : COLOR(190, 150, 100);
        setcolor(rootColor);

        int thickness = std::max(1, static_cast<int>(depth * depth * 0.35 * zoomScale * growthProgress));
        setlinestyle(SOLID_LINE, 0, thickness);

        double currentLength = length * growthProgress;

        // Horizontal stretch (1.3) and vertical compression (0.5) for an organic spread
        int x2 = x1 + static_cast<int>(currentLength * cos(angle) * 1.3);
        int y2 = y1 + static_cast<int>(currentLength * sin(angle) * 0.5);

        // Surface Guard
        int drawY1 = std::max(y1, surfaceLimitY);
        int drawY2 = std::max(y2, surfaceLimitY);

        if (drawY1 >= surfaceLimitY) {
            line(x1, drawY1, x2, drawY2);
        }

        if (depth > 1) {
            // 1. Standard Downward Roots (The deep system)
            drawRoot(x2, y2, length * 0.65, angle + 0.3, depth - 1, growthProgress, surfaceLimitY);
            drawRoot(x2, y2, length * 0.65, angle - 0.3, depth - 1, growthProgress, surfaceLimitY);

            // 2. Standard Lateral Roots (Side spread)
            drawRoot(x2, y2, length * 0.8, angle + 1.3, depth - 1, growthProgress, surfaceLimitY);
            drawRoot(x2, y2, length * 0.8, angle - 1.3, depth - 1, growthProgress, surfaceLimitY);

            // --- THE TWO NEW SURFACE BRANCHES (Angled slightly below) ---
            if (depth == 4) {
                // Right-side surface root: 0.2 is slightly "down" from horizontal
                drawRoot(x1, y1, length * 1.2, 0.2, depth - 2, growthProgress, surfaceLimitY);

                // Left-side surface root: PI - 0.2 is slightly "down" from the left horizontal
                drawRoot(x1, y1, length * 1.2, PI - 0.2, depth - 2, growthProgress, surfaceLimitY);
            }
        }
    }

   public:
    AnimatedTreeDrawer()
        : isPaused(true),
          screenWidth(800),
          screenHeight(600),
          groundLevel(480),
          seedX(400),
          seedY(groundLevel + 25),
          sunAngle(0.0),
          treeGrowthScale(0.0),
          flowerScale(0.0),
          showFlowers(false),
          animationPhase(0),
          phaseTimer(0),
          rightmostBranchX(0),
          rightmostBranchY(0),
          zoomScale(1.0),
          cameraOffsetX(0),
          cameraOffsetY(0) {}

    void initialize() {
        lastTime = std::chrono::steady_clock::now();
        initwindow(screenWidth, screenHeight, "Animated Tree Life Cycle");
        setbkcolor(SKY_BLUE);
        cleardevice();
        setactivepage(0);
        setvisualpage(0);
    }

    void resetAnimation() {
        treeGrowthScale = 0.0;
        flowerScale = 0.0;
        showFlowers = false;
        animationPhase = 0;
        phaseTimer = 0;
        fallingSeeds.clear();
        seedX = screenWidth / 2;
        seedY = groundLevel + 25;
        zoomScale = 1.0;
        cameraOffsetX = 0;
        cameraOffsetY = 0;
        flowerPositions.clear();
        rightmostBranchX = 0;
        rightmostBranchY = 0;
        fallingSeeds.clear();
    }

    void update() {
        phaseTimer += 1;

        switch (animationPhase) {
            case 0:  // Seed germination (0-40 frames)
                if (phaseTimer < 40) {
                    treeGrowthScale = 0.0;
                } else {
                    animationPhase = 1;
                    phaseTimer = 0;
                }
                break;

            case 1: {  // Leaf phase
                if (phaseTimer < 60) {
                    treeGrowthScale = std::min(0.15, phaseTimer / 400.0);
                } else {
                    animationPhase = 2;
                    phaseTimer = 0;
                }
                break;
            }

            case 2: {  // Tree growth
                if (phaseTimer < 100) {
                    treeGrowthScale = 0.15 + (phaseTimer / 100.0) * 0.85;
                } else {
                    treeGrowthScale = 1.0;  // HARD CAP: Prevents infinite growth
                    animationPhase = 3;
                    phaseTimer = 0;
                    showFlowers = true;
                }
                break;
            }

            case 3: {  // Flowering
                if (phaseTimer < 100) {
                    flowerScale = phaseTimer / 100.0;
                } else {
                    animationPhase = 4;
                    phaseTimer = 0;

                    Seed newSeed;
                    newSeed.x = seedX + 150;
                    // CHANGE: Decreased from -200 to -300 to make it spawn much higher
                    newSeed.y = seedY - 360;

                    newSeed.angle = 60;
                    newSeed.velocityY = 0;
                    newSeed.velocityX = 0.8;
                    newSeed.active = true;
                    fallingSeeds.clear();
                    fallingSeeds.push_back(newSeed);
                }
                break;
            }

            case 4: {  // Falling phase
                updateFallingSeeds();

                if (!fallingSeeds.empty()) {
                    Seed& s = fallingSeeds[0];

                    // 1. Keep your original cinematic zoom speed
                    if (zoomScale < 8.0) zoomScale += 0.05;

                    // 2. Define the starting focus (Center of Tree) and target focus (Seed)
                    double startFocusX = screenWidth / 2.0;
                    double startFocusY = screenHeight / 2.0;
                    double targetFocusX = s.x;
                    double targetFocusY = s.y;

                    // 3. Calculate camera panning progress (0.0 to 1.0)
                    // Using phaseTimer / 80.0 means the pan finishes smoothly around frame 80,
                    // ensuring the camera locks onto the seed while it's still in the air.
                    double panProgress = std::min(1.0, phaseTimer / 80.0);

                    // Add a cinematic "ease-out" curve. The pan moves fast initially to catch
                    // the seed, then gently slows down as it locks on.
                    double ease = 1.0 - pow(1.0 - panProgress, 3);

                    // 4. Calculate the current point the camera should look at
                    double currentFocusX = startFocusX + (targetFocusX - startFocusX) * ease;
                    double currentFocusY = startFocusY + (targetFocusY - startFocusY) * ease;

                    // 5. THE MAGIC FORMULA: Keep currentFocus perfectly centered at the current zoomScale
                    cameraOffsetX = (screenWidth / 2.0 / zoomScale) - currentFocusX;
                    cameraOffsetY = (screenHeight / 2.0 / zoomScale) - currentFocusY;

                    // Trigger Phase 5 once landed
                    if (s.y >= groundLevel + 25) {
                        // Wait for a small pause before zooming out
                        if (phaseTimer > 100) {
                            animationPhase = 5;
                            phaseTimer = 0;
                        }
                    }
                }
                break;
            }

            case 5: {                    // Zoom out phase
                if (phaseTimer < 150) {  // Give it 150 frames to zoom out
                    double progress = phaseTimer / 150.0;

                    // Smoothly transition zoom from 8.0 back to 1.0
                    zoomScale = 8.0 - (7.0 * progress);

                    if (!fallingSeeds.empty()) {
                        Seed& s = fallingSeeds[0];

                        // X Axis: Stay centered on seed
                        cameraOffsetX = (screenWidth / 2.0 / zoomScale) - s.x;

                        // Y Axis: Transition from "Center of Screen" to "Landed Position"
                        double startScreenY = screenHeight / 2.0;
                        double endScreenY = (groundLevel + 25.0);
                        double currentScreenY = startScreenY + (endScreenY - startScreenY) * progress;

                        cameraOffsetY = (currentScreenY / zoomScale) - s.y;
                    }

                    // Fade out the old tree scale
                    treeGrowthScale = 1.0 - progress;
                } else {
                    // HARD RESET to ensure next cycle starts clean
                    zoomScale = 1.0;
                    cameraOffsetX = 0;
                    cameraOffsetY = 0;
                    if (!fallingSeeds.empty()) seedX = fallingSeeds[0].x;
                    resetAnimation();
                }
                break;
            }
        }

        auto currentTime = std::chrono::steady_clock::now();
        double elapsedSeconds = std::chrono::duration<double>(currentTime - lastTime).count();
        lastTime = currentTime;

        sunAngle += 0.5 * elapsedSeconds;  // radians per second
        if (sunAngle > 2 * PI) sunAngle -= 2 * PI;
    }

    void render() {
        setactivepage(1 - getactivepage());

        // 1. ENVIRONMENT (Sky, Sun, Clouds)
        int r = 100 - static_cast<int>(50 * -sin(sunAngle));
        int g = 170 - static_cast<int>(100 * -sin(sunAngle));
        int b = 200 - static_cast<int>(80 * -sin(sunAngle));
        r = std::max(0, std::min(255, r));
        g = std::max(0, std::min(255, g));
        b = std::max(0, std::min(255, b));
        setbkcolor(COLOR(r, g, b));
        cleardevice();

        drawSun();
        drawClouds();

        // 2. COORDINATE TRANSFORMATIONS
        int visualGroundY = static_cast<int>((groundLevel + cameraOffsetY) * zoomScale);
        int anchorX = static_cast<int>((seedX + cameraOffsetX) * zoomScale);
        int anchorY = static_cast<int>((seedY + cameraOffsetY) * zoomScale);

        // 3. SOIL (The furthest background layer)
        int tempGround = groundLevel;
        groundLevel = visualGroundY;
        drawSoil();
        groundLevel = tempGround;

        // 4. CHECK FOR NEW GENERATION
        bool newSeedHasLanded = false;
        if (!fallingSeeds.empty()) {
            for (const auto& s : fallingSeeds) {
                if (!s.active && s.y >= groundLevel - 1) {
                    newSeedHasLanded = true;
                    break;
                }
            }
        }

        // --- 5. FALLING SEEDS (Drawn BEHIND the tree) ---
        // Moving this here ensures branches/leaves are drawn over the falling seeds.
        for (const auto& s : fallingSeeds) {
            int sx = static_cast<int>((s.x + cameraOffsetX) * zoomScale);
            int sy = static_cast<int>((s.y + cameraOffsetY) * zoomScale);

            // We draw them even if active or landed to ensure seamless transition
            drawSeed(sx, sy, s.angle, 1.2 * zoomScale);
        }

        // 6. THE PARENT TREE & ROOT SYSTEM (Drawn IN FRONT of falling seeds)
        if (!newSeedHasLanded) {
            // --- ROOTS & CONNECTOR ---
            if (animationPhase >= 1 && animationPhase <= 4) {
                setcolor(COLOR(140, 100, 60));
                int connectionWidth = std::max(1, static_cast<int>(5 * zoomScale * treeGrowthScale));
                setlinestyle(SOLID_LINE, 0, connectionWidth);

                int bridgeTopY = anchorY - static_cast<int>((anchorY - visualGroundY) * treeGrowthScale);
                int finalTopY = std::max(bridgeTopY, visualGroundY);

                line(anchorX, anchorY, anchorX, finalTopY);
                drawRoot(anchorX, anchorY, 80.0 * zoomScale, PI / 2.0, 4, treeGrowthScale, visualGroundY);
            }

            // --- TRUNK & CANOPY ---
            // This is the main visual body that will now occlude the seeds
            if (animationPhase >= 2 || (animationPhase == 1 && phaseTimer > 50)) {
                double blendFactor = (animationPhase == 1) ? (phaseTimer - 50) / 10.0 : 1.0;
                setcolor(DARK_BROWN);
                if (treeGrowthScale > 0.01) {
                    drawBranch(anchorX, visualGroundY, 150 * zoomScale, PI / 2.0, 5, treeGrowthScale * blendFactor, treeGrowthScale);
                }
            }

            // --- THE VANISHING INITIAL SEED ---
            if (animationPhase <= 2) {
                double morphFactor = std::max(0.0, 1.0 - (treeGrowthScale * 5.0));

                if (morphFactor > 0.01) {
                    double currentScale = (animationPhase == 0 ? 1.0 + phaseTimer / 20.0 : 2.0) * morphFactor * zoomScale;
                    drawSeed(anchorX, anchorY, 0, currentScale);
                }
            }

            // --- SPROUT & LEAVES ---
            if (animationPhase == 0 && phaseTimer > 20) {
                setcolor(LIGHT_GREEN);
                setlinestyle(SOLID_LINE, 0, 2);
                double sproutProgress = (phaseTimer - 20) / 20.0;
                int sproutLength = static_cast<int>(sproutProgress * 20 * zoomScale);
                line(anchorX, anchorY, anchorX, anchorY - sproutLength);
            }
            if (animationPhase == 1) {
                drawSeedlingLeaves(anchorX, anchorY, phaseTimer / 60.0);
            }
        }

        displayPhaseInfo();
        if (isPaused) {
            drawPauseMenu();
        }
        setvisualpage(getactivepage());
    }

    void run() {
        initialize();
        isPaused = true;  // Ensure we start paused

        while (true) {
            if (kbhit()) {
                char key = getch();
                if (key == 'q' || key == 'Q') break;
                if (key == 'r' || key == 'R') {
                    resetAnimation();
                    isPaused = false;  // Optional: Auto-play on reset
                }
                if (key == ' ') isPaused = !isPaused;
            }

            if (!isPaused) {
                update();
            } else {
                // THE SUN TELEPORTATION FIX:
                // We refresh lastTime even while paused. When we resume,
                // the delta in update() will only be ~33ms instead of the
                // entire duration of the pause.
                lastTime = std::chrono::steady_clock::now();
            }

            render();
            delay(33);
        }
        closegraph();
    }
};

int main() {
    AnimatedTreeDrawer drawer;
    drawer.run();

    return 0;
}
