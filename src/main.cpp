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
    int cameraOffsetX, cameraOffsetY;

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

    // Draw soil layers starting at a specific visual Y position
    void drawSoil(int visualGroundY) {
        // Ground surface
        setcolor(DARK_BROWN);
        setfillstyle(SOLID_FILL, DARK_BROWN);
        // Draw from the calculated visual Y down to the bottom of the screen
        bar(0, visualGroundY, screenWidth, screenHeight);

        // Underground soil (darker) - starts 50 pixels below the surface (scaled?)
        // actually, let's just make the deep soil start a bit lower
        setcolor(SOIL_BROWN);
        setfillstyle(SOLID_FILL, SOIL_BROWN);
        bar(0, visualGroundY + 50, screenWidth, screenHeight);
    }

    // Draw a branch recursively with scaling
    void drawBranch(int x1, int y1, double length, double angle, int depth, double scale, double growthProgress = 1.0) {
        // OPTIMIZATION 1: Cull branches that are too small to see.
        // If the branch is less than 2 pixels long, don't waste CPU drawing it.
        double currentVisualLength = length * scale;
        if (currentVisualLength < 2.0 || depth <= 0) return;

        double branchProgress = std::min(1.0, std::max(0.0, (growthProgress * 10) - (8 - depth)));
        if (branchProgress <= 0) return;

        double scaledLength = currentVisualLength * branchProgress;

        int x2 = x1 + static_cast<int>(scaledLength * cos(angle));
        int y2 = y1 - static_cast<int>(scaledLength * sin(angle));

        // Track rightmost position for flower/seed
        if (depth == 1 && x2 > rightmostBranchX) {
            rightmostBranchX = x2;
            rightmostBranchY = y2;
        }

        // Draw the Branch
        if (depth > 4) {
            setcolor(BROWN);
            setlinestyle(SOLID_LINE, 0, static_cast<int>(depth * scale) + 1);
        } else {
            setcolor(COLOR(101, 67, 33));
            // OPTIMIZATION 2: Simpler lines for small branches
            setlinestyle(SOLID_LINE, 0, std::max(1, static_cast<int>(depth * scale)));
        }

        line(x1, y1, x2, y2);

        // OPTIMIZATION 3: Reduce leaf density
        // Only draw complex leaves if the branch is large enough
        if (depth <= 5 && scale > 0.5 && branchProgress > 0.8 && currentVisualLength > 5.0) {
            setcolor(LIGHT_GREEN);
            setfillstyle(SOLID_FILL, LIGHT_GREEN);

            // Draw fewer leaves at lower depths to save FPS
            int numLeaves = (depth <= 2) ? 5 : 2;

            for (int i = 0; i < numLeaves; i++) {
                int leafX = x2 + (rand() % 12 - 6);
                int leafY = y2 + (rand() % 12 - 6);
                // Simplify leaf size calc
                int leafSize = static_cast<int>(3 * scale);
                if (leafSize > 0) fillellipse(leafX, leafY, leafSize, leafSize);
            }
        }

        // Draw flowers only at depth 1 (Tips)
        if (depth == 1 && showFlowers && scale > 0.8 && branchProgress > 0.9) {
            drawFlower(x2, y2, flowerScale);
        }

        double newLength = length * 0.7;

        // Recursive calls
        drawBranch(x2, y2, newLength, angle - 0.3, depth - 1, scale, growthProgress);
        drawBranch(x2, y2, newLength, angle + 0.3, depth - 1, scale, growthProgress);
        drawBranch(x2, y2, newLength * 0.8, angle, depth - 1, scale, growthProgress);
    }

    void drawFlower(int x, int y, double scale) {
        if (scale <= 0) return;

        // Collect all flower positions
        Point flowerPos;
        flowerPos.x = x;
        flowerPos.y = y;
        flowerPositions.push_back(flowerPos);

        int petalSize = static_cast<int>(4 * scale);

        setcolor(COLOR(255, 192, 203));
        setfillstyle(SOLID_FILL, COLOR(255, 192, 203));

        for (int i = 0; i < 5; i++) {
            double angle = i * 2 * 3.14159 / 5;
            int petalX = x + static_cast<int>(petalSize * cos(angle));
            int petalY = y + static_cast<int>(petalSize * sin(angle));
            fillellipse(petalX, petalY, petalSize, petalSize);
        }

        setcolor(YELLOW);
        setfillstyle(SOLID_FILL, YELLOW);
        fillellipse(x, y, petalSize - 1, petalSize - 1);
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

            if (seed.y < groundLevel) {  // IN AIR
                seed.velocityY += 0.05;
                seed.y += seed.velocityY;
                seed.x += seed.velocityX;
                seed.angle += 0.15;  // Constant tumble
            } else {                 // GROUND PHASE
                // Friction
                seed.velocityX *= 0.8;

                // ROTATION FIX: Smoothly straighten up
                double normalizedAngle = fmod(seed.angle, 6.283);
                if (std::abs(normalizedAngle) > 0.05) {
                    seed.angle += (6.283 - normalizedAngle) * 0.1;
                } else {
                    seed.angle = 0;  // Snap to 0 when close enough
                }

                // SINKING & HARD STOP
                if (seed.y < groundLevel + 25) {
                    seed.y += 0.2;  // Sink slowly
                } else {
                    // FORCE STOP: This kills the vibration
                    seed.y = groundLevel + 25;
                    seed.velocityY = 0;
                    seed.velocityX = 0;
                    // Don't change 'active' to false, or it will vanish!
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
        // progress goes from 0 to 1
        int stemHeight = static_cast<int>(60 * progress);  // Increased from 30

        // Draw stem (this becomes the trunk)
        setcolor(LEAF_GREEN);
        setlinestyle(SOLID_LINE, 0, std::max(2, static_cast<int>(progress * 4)));
        line(x, y, x, y - stemHeight);

        // Leaves appear and grow
        if (progress > 0.2) {  // Changed from 0.3 to appear earlier
            double leafProgress = (progress - 0.2) / 0.8;
            int leafSize = static_cast<int>(20 * leafProgress);    // Increased from 15
            int leafYOffset = static_cast<int>(stemHeight * 0.5);  // Position leaves partway up stem

            setcolor(LIGHT_GREEN);
            setfillstyle(SOLID_FILL, LIGHT_GREEN);

            // Left leaf - angled outward
            fillellipse(x - leafSize, y - leafYOffset, leafSize, static_cast<int>(leafSize * 0.6));

            // Right leaf - angled outward
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
        phaseTimer += 3;

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
                if (phaseTimer < 25) {
                    flowerScale = phaseTimer / 25.0;
                } else {
                    animationPhase = 4;
                    phaseTimer = 0;

                    Seed newSeed;
                    newSeed.x = seedX + 150;
                    // CHANGE: Decreased from -200 to -300 to make it spawn much higher
                    newSeed.y = seedY - 390;

                    newSeed.angle = 0;
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
                    if (zoomScale < 8.0) zoomScale += 0.03;

                    // Camera centers on the seed.
                    // Since the seed is to the right of the tree,
                    // the tree is pushed to the left of the screen.
                    cameraOffsetX = (screenWidth / 2.0 / zoomScale) - s.x;
                    cameraOffsetY = (screenHeight / 2.0 / zoomScale) - s.y;

                    // Transition to zoom-out once seed is upright and buried
                    if (s.y >= groundLevel + 25 && std::abs(fmod(s.angle, 6.28)) < 0.1) {
                        animationPhase = 5;
                        phaseTimer = 0;
                    }
                }
                break;
            }

            case 5: {
                if (phaseTimer < 150) {
                    double progress = phaseTimer / 150.0;
                    zoomScale = 8.0 - (7.0 * progress);

                    if (!fallingSeeds.empty()) {
                        Seed& s = fallingSeeds[0];

                        cameraOffsetX = (screenWidth / 2.0 / zoomScale) - s.x;

                        // CHANGE: Changed 0.8 to 0.65.
                        // This moves the "fixed ground line" higher up the screen,
                        // keeping the seed safely away from the bottom border.
                        cameraOffsetY = (screenHeight * 0.65 / zoomScale) - groundLevel;
                    }

                    treeGrowthScale = 0;

                } else {
                    // SEAMLESS TRANSITION: Force zoomScale and offsets to 1.0
                    // right before reset to avoid any "snapping" or "jumping"
                    zoomScale = 1.0;
                    cameraOffsetX = 0;
                    cameraOffsetY = 0;

                    if (!fallingSeeds.empty()) {
                        seedX = fallingSeeds[0].x;
                    }
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

        // --- CRITICAL FIX: UNIFIED GROUND CALCULATION ---
        // We calculate the screen Y position for the ground level ONCE.
        // This ensures the tree and the soil are mathematically locked together.
        int visualGroundY = static_cast<int>((groundLevel + cameraOffsetY) * zoomScale);

        // Draw Soil using the calculated visual Y
        // (Note: You need to update drawSoil to accept this int parameter as discussed before)
        drawSoil(visualGroundY);

        // Draw seed underground (Phases 0-1)
        if (animationPhase <= 1) {
            double seedScale = 1.0 + (animationPhase == 0 ? phaseTimer / 20.0 : 2.0);

            // Calculate seed position relative to the camera
            int seedDrawX = static_cast<int>((seedX + cameraOffsetX) * zoomScale);
            int seedDrawY = static_cast<int>((seedY + cameraOffsetY) * zoomScale);

            drawSeed(seedDrawX, seedDrawY, 0, seedScale * zoomScale);

            if (animationPhase == 0 && phaseTimer > 20) {
                setcolor(LIGHT_GREEN);
                double sproutProgress = (phaseTimer - 20) / 20.0;
                int sproutLength = static_cast<int>(sproutProgress * 20 * zoomScale);
                line(seedDrawX, seedDrawY, seedDrawX, seedDrawY - sproutLength);
            }
        }

        // Draw Seedling (Phase 1)
        if (animationPhase == 1) {
            double leafProgress = phaseTimer / 60.0;
            int leafDrawX = static_cast<int>((seedX + cameraOffsetX) * zoomScale);

            // Interpolate Y from seed position to ground level
            double baseY = seedY + leafProgress * (groundLevel - seedY);
            int leafDrawY = static_cast<int>((baseY + cameraOffsetY) * zoomScale);

            drawSeedlingLeaves(leafDrawX, leafDrawY, leafProgress);
        }

        // TREE DRAWING
        // Only draw if we are in growth phase OR fading out (Phase 5) but NOT if scale is 0
        if ((animationPhase >= 2 || (animationPhase == 1 && phaseTimer > 50)) && treeGrowthScale > 0.01) {
            double blendFactor = 1.0;
            if (animationPhase == 1) {
                blendFactor = (phaseTimer - 50) / 10.0;
            }

            // USE THE SAME COORDINATES AS THE GROUND
            int startX = static_cast<int>((seedX + cameraOffsetX) * zoomScale);
            int startY = visualGroundY;  // Locked to soil

            int trunkLength = static_cast<int>(150 * zoomScale);
            double initialAngle = 3.14159 / 2;

            drawBranch(startX, startY, trunkLength, initialAngle, 6, treeGrowthScale * blendFactor, treeGrowthScale * blendFactor);
        }

        // Draw falling seeds
        for (const auto& seed : fallingSeeds) {
            if (seed.active) {
                int drawX = static_cast<int>((seed.x + cameraOffsetX) * zoomScale);
                int drawY = static_cast<int>((seed.y + cameraOffsetY) * zoomScale);

                // CHANGE: Changed multiplier from 2.0 to 1.0.
                // This ensures that when zoomScale reaches 1.0 at the end of the
                // animation, the seed is the exact same size as the next
                // seed that starts the growth phase.
                drawSeed(drawX, drawY, seed.angle, zoomScale * 1.0);
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
