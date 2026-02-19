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
    double velocityX;  // ADD THIS
    bool active;
};

class AnimatedTreeDrawer {
   private:
    int screenWidth, screenHeight;
    int groundLevel;
    int seedX, seedY;
    std::chrono::steady_clock::time_point lastTime;
    double sunAngle;  // for moving sun

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
    const int BROWN = COLOR(139, 69, 19);
    const int DARK_BROWN = COLOR(101, 67, 33);
    const int LEAF_GREEN = COLOR(34, 139, 34);
    const int LIGHT_GREEN = COLOR(50, 205, 50);
    const int SKY_BLUE = COLOR(135, 206, 235);
    const int SOIL_BROWN = COLOR(90, 50, 20);

    // Draw a seed with rotation
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
            double t = i * 2 * 3.14159 / numPoints;
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
        setcolor(DARK_BROWN);
        setfillstyle(SOLID_FILL, DARK_BROWN);
        // Draw from the current visual level to the screen bottom
        bar(0, groundLevel, screenWidth, screenHeight);

        setcolor(SOIL_BROWN);
        setfillstyle(SOLID_FILL, SOIL_BROWN);
        bar(0, groundLevel + 40, screenWidth, screenHeight);
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

        // Save the color so the branches don't turn yellow!
        int oldColor = getcolor();

        // Size reduced: Changed petal size from 4 to 2 for smaller blossoms
        int petalSize = static_cast<int>(3 * scale);

        // Draw Petals (Pink)
        setcolor(COLOR(255, 192, 203));
        setfillstyle(SOLID_FILL, COLOR(255, 192, 203));

        for (int i = 0; i < 5; i++) {
            double angle = i * 2 * 3.14159 / 5;
            int petalX = x + static_cast<int>(petalSize * cos(angle));
            int petalY = y + static_cast<int>(petalSize * sin(angle));
            fillellipse(petalX, petalY, petalSize, petalSize);
        }

        // Draw Center (Yellow)
        setcolor(YELLOW);
        setfillstyle(SOLID_FILL, YELLOW);
        fillellipse(x, y, std::max(1, petalSize / 2), std::max(1, petalSize / 2));

        // Restore the branch color
        setcolor(oldColor);
    }

    // Draw the sun
    void drawSun() {
        int radius = 30;
        int skyHeight = 150;
        int sunX = static_cast<int>(screenWidth * sunAngle / 3.14159);
        int sunY = static_cast<int>(skyHeight * sin(sunAngle)) + 50;

        setcolor(YELLOW);
        setfillstyle(SOLID_FILL, YELLOW);
        fillellipse(sunX, sunY, radius, radius);

        for (int i = 0; i < 12; i++) {
            double angle = i * 30 * 3.14159 / 180.0;
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
                seed.angle = (distanceToGround / 35.0) + (3.14159 * 4);
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
        setcolor(WHITE);
        settextstyle(DEFAULT_FONT, HORIZ_DIR, 2);

        char title[100];
        switch (animationPhase) {
            case 0:
                sprintf(title, "Phase 1: Seed Germination");
                break;
            case 1:
                sprintf(title, "Phase 2: Seedling (Leaves)");
                break;
            case 2:
                sprintf(title, "Phase 3: Tree Growth");
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
        outtextxy(10, 10, title);

        settextstyle(DEFAULT_FONT, HORIZ_DIR, 1);
        char msg[] = "Press ESC to exit, SPACE to restart";
        outtextxy(10, screenHeight - 20, msg);
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

   public:
    AnimatedTreeDrawer()
        : screenWidth(800),
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

            case 1: {  // Leaf phase (0-60 frames) - grows smoothly
                if (phaseTimer < 60) {
                    // Don't set to 0, let it transition smoothly
                    treeGrowthScale = std::min(0.15, phaseTimer / 400.0);  // Very gradual start
                } else {
                    animationPhase = 2;
                    phaseTimer = 0;
                }
                break;
            }

            case 2: {  // Tree growth (0-100 frames)
                if (phaseTimer < 100) {
                    treeGrowthScale = 0.15 + (phaseTimer / 100.0) * 0.85;  // Continue from 0.15
                } else {
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
        if (sunAngle > 2 * 3.14159) sunAngle -= 2 * 3.14159;
    }

    void render() {
        setactivepage(1 - getactivepage());

        // Sky Color Logic
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

        // --- CRITICAL FIX: UNIFIED TRANSFORMATION ---
        // Calculate the ground level in screen space using the SHARED camera math
        int visualGroundY = static_cast<int>((groundLevel + cameraOffsetY) * zoomScale);

        // Temporarily set groundLevel for drawSoil
        int tempGround = groundLevel;
        groundLevel = visualGroundY;
        drawSoil();
        groundLevel = tempGround;

        // Draw seed (Underground or Falling)
        if (animationPhase <= 1 || animationPhase >= 4) {
            double currentX, currentY, currentAngle, currentScale;

            if (!fallingSeeds.empty() && fallingSeeds[0].active) {
                // Falling state
                currentX = fallingSeeds[0].x;
                currentY = fallingSeeds[0].y;
                currentAngle = fallingSeeds[0].angle;
                currentScale = zoomScale * 1.0;  // Use 1.0 to match the seedling size later
            } else {
                // Landed/Germinating state
                currentX = seedX;
                currentY = seedY;
                currentAngle = 0;
                currentScale = (animationPhase == 0 ? 1.0 + phaseTimer / 20.0 : 2.0) * zoomScale;
            }

            // Apply the EXACT SAME formula to the seed as the ground
            int drawX = static_cast<int>((currentX + cameraOffsetX) * zoomScale);
            int drawY = static_cast<int>((currentY + cameraOffsetY) * zoomScale);

            drawSeed(drawX, drawY, currentAngle, currentScale);

            // --- FIX: DRAW SPROUT (Phase 0) ---
            if (animationPhase == 0 && phaseTimer > 20) {
                setcolor(LIGHT_GREEN);
                setlinestyle(SOLID_LINE, 0, 2);
                double sproutProgress = (phaseTimer - 20) / 20.0;
                int sproutLength = static_cast<int>(sproutProgress * 20 * zoomScale);
                line(drawX, drawY, drawX, drawY - sproutLength);
            }

            // Draw Seedling (Phase 1)
            if (animationPhase == 1) {
                double leafProgress = phaseTimer / 60.0;
                int leafDrawX = static_cast<int>((seedX + cameraOffsetX) * zoomScale);

                // Seedling grows from the seed position
                int leafDrawY = static_cast<int>((seedY + cameraOffsetY) * zoomScale);

                drawSeedlingLeaves(leafDrawX, leafDrawY, leafProgress);
            }
            // -------------------------------------------
        }

        // Tree Drawing
        if (animationPhase >= 2 || (animationPhase == 1 && phaseTimer > 50)) {
            double blendFactor = (animationPhase == 1) ? (phaseTimer - 50) / 10.0 : 1.0;
            int startX = static_cast<int>((seedX + cameraOffsetX) * zoomScale);
            int startY = visualGroundY;  // Perfectly locked to the soil

            if (treeGrowthScale > 0.01) {
                drawBranch(startX, startY, 150 * zoomScale, 3.14159 / 2, 5, treeGrowthScale * blendFactor, treeGrowthScale * blendFactor);
            }
        }

        displayPhaseInfo();
        setvisualpage(getactivepage());
    }

    void run() {
        initialize();

        while (true) {
            // Check for keyboard input
            if (kbhit()) {
                char key = getch();
                if (key == 27) break;              // ESC to exit
                if (key == ' ') resetAnimation();  // SPACE to restart
            }

            // Update animation state
            update();

            // Render frame
            render();

            // Control frame rate (~30 FPS)
            // Sleep(33);  // Windows Sleep uses milliseconds
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
