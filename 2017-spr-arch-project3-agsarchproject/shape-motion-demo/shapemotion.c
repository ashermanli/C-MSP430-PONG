/** \file shapemotion.c
 *  \brief This is a simple shape motion demo.
 *  This demo creates two layers containing shapes.
 *  One layer contains a rectangle and the other a circle.
 *  While the CPU is running the green LED is on, and
 *  when the screen does not need to be redrawn the CPU
 *  is turned off along with the green LED.
 */  
#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <p2switches.h>
#include <shape.h>
#include <abCircle.h>
#include <stdlib.h>
#include "buzzer.h"


#define GREEN_LED BIT6

static int y = 0;

//velocity vectors for the ball
static int yy = -2;
static int x = 1;

static int score = 0;
static int highscore = 0;
static int lives = 3;


u_int bgColor = COLOR_BLACK;     /**< The background color */
int redrawScreen = 1;           /**< Boolean for whether screen needs to be redrawn */

Region fieldFence;		/**< fence around playing field  */


//AbRect rect10 = {abRectGetBounds, abRectCheck, {10,10}}; /** paddle 1 */
AbRect rect20 = {abRectGetBounds, abRectCheck, {2,15}}; // paddle 2

AbRectOutline fieldOutline = {	/* playing field */
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2-1, screenHeight/2-1}
};

Layer paddle = {
  (AbShape *)&rect20,
  {(screenWidth/2)-58, (screenHeight/2)+5}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_GREEN,
  0
};
 
Layer ball = {		/**< Layer with an violet circle */
  (AbShape *)&circle4,
  {(screenWidth/2)+10, (screenHeight/2)+5}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_RED,
  &paddle,
};


Layer fieldLayer = {		/* playing field as a layer */
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2},/**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLACK,
  &ball
};

/** Moving Layer
 *  Linked list of layer references
 *  Velocity represents one iteration of change (direction & magnitude)
 */
typedef struct MovLayer_s {
  Layer *layer;
  Vec2 velocity;
  struct MovLayer_s *next;
} MovLayer;

MovLayer ml0 = {&ball, {2,2}, 0};
MovLayer m20 = {&paddle, {0,0}, &ml0};

void movLayerDraw(MovLayer *movLayers, Layer *layers)
{
  int row, col;
  MovLayer *movLayer;

  and_sr(~8);			/**< disable interrupts (GIE off) */
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Layer *l = movLayer->layer;
    l->posLast = l->pos;
    l->pos = l->posNext;
  }
  or_sr(8);			/**< disable interrupts (GIE on) */


  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Region bounds;
    layerGetBounds(movLayer->layer, &bounds);
    lcd_setArea(bounds.topLeft.axes[0], bounds.topLeft.axes[1], 
		bounds.botRight.axes[0], bounds.botRight.axes[1]);
    for (row = bounds.topLeft.axes[1]; row <= bounds.botRight.axes[1]; row++) {
      for (col = bounds.topLeft.axes[0]; col <= bounds.botRight.axes[0]; col++) {
	Vec2 pixelPos = {col, row};
	u_int color = bgColor;
	Layer *probeLayer;
	for (probeLayer = layers; probeLayer; 
	     probeLayer = probeLayer->next) { /* probe all layers, in order */
	  if (abShapeCheck(probeLayer->abShape, &probeLayer->pos, &pixelPos)) {
	    color = probeLayer->color;
	    break; 
	  } /* if probe check */
	} // for checking all layers at col, row
	lcd_writeColor(color); 
      } // for col
    } // for row
  } // for moving layer being updated
}	  

u_char collide(Vec2* p, Region* r)
{
  if(p->axes[0] < (r->topLeft.axes[0])+5 || p->axes[0] > (r->botRight.axes[0])+5)
    return 0;
  if(p->axes[1] < (r->topLeft.axes[1])+5 || p->axes[1] > (r->botRight.axes[1])+5)
    return 0;

  return 1;
}
//#define STATE_PLAYING 0
//#define STATE_GAMEOVER 1
static char current_state = 0;

static char tmDown = 0;
void makeNoise()
{
  tmDown = 5;
  buzzer_set_period(250);
}

//checks for button presses to move the paddle
void checkInput()
{
      // m20
    // check input
    unsigned char isS1 = (p2sw_read() & 1) ? 0 : 1;
    unsigned char isS2 = (p2sw_read() & 8) ? 0 : 1;
    if(isS1){
      y = -7;
    }
    else if(isS2){
      y = 7;
    }
    else{
      y = 0;
    }
    Vec2 newVelocity = {0, y};
    (&m20)->velocity = newVelocity;
}

//Credit to Jose Perez
//www.github.com/DeveloperJose
//Checks if the ball collides with the paddle region
void doCollision()
{
    MovLayer *paddleLayer = &m20;
    Region paddleBoundary;
    layerGetBounds((paddleLayer->layer), &paddleBoundary);
    //(&paddleBoundary)->topLeft.axes[0] -= 5;
    
    //Code to collide ball w/ paddle
    Vec2 pos = (&ball)->pos;
    Vec2 iniVelocity = (&ml0)->velocity;
    Vec2 bounceVelocity = {x,yy};

    //sets proper y velocity vectors
    if(collide(&pos, &paddleBoundary)){
      if((&iniVelocity)->axes[0] < 0 && (&iniVelocity)->axes[1] <= 0){
	x = 2;
	yy = -2;
      }
      else if ((&iniVelocity)->axes[0] < 0 && (&iniVelocity)->axes[1] > 0)
	{
	  x = 2;
	  yy = 2;
	}

      (&ml0)->velocity = bounceVelocity;
      
      makeNoise();
      scoreUpdate();
      scoreDraw();
    }
     mlAdvance(&m20, &fieldFence);
  
}

char buffer[4];
char buffer2[4];
void scoreUpdate()
{
  itoa(score, buffer, 10);
  itoa(highscore, buffer2,10);
  if(score > highscore){
    highscore = score;
    highscore -=50;
  }
}

void scoreDraw()
{
  drawString5x7(50,10, "    ", COLOR_GREEN, COLOR_BLACK);
  drawString5x7(50,10, buffer, COLOR_GREEN, COLOR_BLACK);
  score+=50;
}


//When playing the game
void onPlayingState(){
  checkInput();
  doCollision();
  tmDown--;
  if(tmDown <= 0)
    buzzer_set_period(0);
  if(lives <= 0)
    current_state = 1;
 
  //moving the ball
  mlAdvance(&ml0, &fieldFence);
  redrawScreen = 1;
}

//When player runs out of lives
void onGameoverState(){
  drawString5x7(50,10, buffer2, COLOR_GREEN, COLOR_BLACK);
  drawString5x7(17,screenHeight/2, "Press any button", COLOR_GREEN, COLOR_BLACK);
  drawString5x7(30,30, "Try again?", COLOR_GREEN, COLOR_BLACK);
  
  unsigned char isS1 = (p2sw_read() & 1) ? 0 : 1;
  unsigned char isS2 = (p2sw_read() & 2) ? 0 : 1;
  unsigned char isS3 = (p2sw_read() & 4) ? 0 : 1;
  unsigned char isS4 = (p2sw_read() & 8) ? 0 : 1;

  if(isS1 || isS2 || isS3 || isS4){
    current_state = 0;
    lives = 3;
    drawString5x7(50,10, "     ", COLOR_GREEN, COLOR_BLACK);
    drawString5x7(17,screenHeight/2, "                ", COLOR_GREEN, COLOR_BLACK);
    drawString5x7(30,30, "          ", COLOR_GREEN, COLOR_BLACK);
    score = 0;
    highscore = 0;
  }


}

//state machine that runs the game
//*void updateState(int state){
/*current_state = state;
  if(current_state == 0)
    onPlayingState();
  else if(current_state == 1)
    onGameoverState();
    }*/
  

//Region fence = {{10,30}, {SHORT_EDGE_PIXELS-10, LONG_EDGE_PIXELS-10}}; /**< Create a fence region */

/** Advances a moving shape within a fence
 *  
 *  \param ml The moving shape to be advanced
 *  \param fence The region which will serve as a boundary for ml
 */
void mlAdvance(MovLayer *ml, Region *fence)
{
  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;
  for (; ml; ml = ml->next) {
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
    for (axis = 0; axis < 2; axis ++) {
            if ((shapeBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis]) ||
        (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis]) ) {
	      int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
	      newPos.axes[axis] += (2*velocity);
            }	/**< if outside of fence */
	    else if(shapeBoundary.topLeft.axes[0] < (fence->topLeft.axes[1])){
	      shapeInit();
	      newPos.axes[0] = screenWidth/2;
	      newPos.axes[1] = screenHeight/2;
	      score = 50;
	      drawString5x7(50,10, "    ", COLOR_GREEN, COLOR_BLACK);
	      lives--; 
	    }
	      
    } /**< for axis */
    ml->layer->posNext = newPos;
  } /**< for ml */
}



/** Initializes everything, enables interrupts and green LED, 
 *  and handles the rendering for the screen
 */
void main()
{
  P1DIR |= GREEN_LED;		/**< Green led on when CPU on */		
  P1OUT |= GREEN_LED;

  configureClocks();
  lcd_init();
  shapeInit();
  p2sw_init(15); //set this to 15 to get buttons working; original is 1

  shapeInit();
  buzzer_init();
  layerInit(&fieldLayer);
  layerDraw(&fieldLayer);

  layerGetBounds(&fieldLayer, &fieldFence);

  enableWDTInterrupts();      /**< enable periodic interrupt */
  or_sr(0x8);	              /**< GIE (enable interrupts) */

  //for loops that redraws all the layers while its moving
  for(;;) { 
    while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */
      P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
      or_sr(0x10);	      /**< CPU OFF */
    }
    P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
    redrawScreen = 0;
    movLayerDraw(&m20, &fieldLayer);
    scoreUpdate();
    drawString5x7(10,10, "score:", COLOR_GREEN, COLOR_BLACK);

    
  }
}





/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler()
{
  static short count = 0;
  P1OUT |= GREEN_LED;		      /**< Green LED on when cpu on */
  count ++;
  if (count == 10) {
    updateState(current_state);
    count = 0;
  } 
  P1OUT &= ~GREEN_LED;		    /**< Green LED off when cpu off */
}
