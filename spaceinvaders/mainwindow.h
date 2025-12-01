#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QVector>
#include <QRect>
#include <QKeyEvent>
#include <QPixmap>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

struct Enemy {
    QRect rect;
    bool alive = true;
};

// NEW: struct for animated player lasers
struct PlayerLaser {
    QRect rect;
    int frame = 0; // used for simple animation/pulse
};

enum class GameState {
    Menu,
    Settings,
    NameEntry,
    Playing,
    GameOver
};

enum class Difficulty {
    Easy,
    Hard
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void updateGame();
    void moveEnemiesRandom();
    void enemiesShoot();

private:
    Ui::MainWindow *ui;

    // Game state
    QRect playerRect;
    QVector<PlayerLaser> playerLasers;  // instead of QVector<QRect> playerBullets
    QVector<QRect> enemyBullets;
    QVector<Enemy> enemies;

    int lives;
    int score;
    int level;
    int highScore;   // single global high score

    bool gameOver;

    bool moveLeft;
    bool moveRight;

    GameState gameState;
    Difficulty difficulty;

    // Menu selections
    int menuSelectionIndex;      // 0: Start Game, 1: Game Settings
    int settingsSelectionIndex;  // 0: Difficulty, 1: Player Name
    int gameOverSelectionIndex;  // 0: Restart, 1: End Game

    // Player name
    QString playerName;
    QString nameInputBuffer;

    // Sprites
    QPixmap backgroundPixmap;
    QPixmap playerPixmap;
    QPixmap enemyPixmap;
    QPixmap playerLaserPixmap;

    // Timers
    QTimer gameTimer;
    QTimer enemyMoveTimer;
    QTimer enemyShootTimer;

    void initGame();
    void setupLevel();
    void resetForNextLevel();
    void resetGameCompletely();

    void updateBullets();
    void handleCollisions();
    void checkLevelComplete();
    void applyDifficulty();

    void loadHighScore();
    void saveHighScore();

    void loadPlayerName();
    void savePlayerName();
};

#endif // MAINWINDOW_H
