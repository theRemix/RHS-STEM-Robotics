#pragma config(Motor,  port2,           xMotor,     tmotorNormal, openLoop)
#pragma config(Motor,  port3,           yMotor,     tmotorNormal, openLoop)
#pragma config(Motor,  port4,           zMotor,     tmotorNormal, openLoop)
#pragma config(Motor,  port5,           pMotor,     tmotorNormal, openLoop)
#pragma config(Sensor, dgtl1, 				  topSensor,    sensorTouch)
#pragma config(Sensor, dgtl2, 				  leftSensor,   sensorTouch)
#pragma config(Sensor, dgtl3, 				  bottomSensor, sensorTouch)
#pragma config(Sensor, dgtl4, 				  rightSensor,  sensorTouch)
#pragma config(Sensor, dgtl5, 				  triggerSensor,sensorTouch)
#pragma config(Sensor, dgtl6, 				  vertSensor,   sensorTouch)
#pragma config(Sensor, dgtl11, 				  modeSensor,   sensorTouch)
#pragma config(Sensor, dgtl12, 				  killSensor,   sensorTouch)

#pragma DebuggerWindows("Locals")
#pragma DebuggerWindows("debugStream")

/*

	digital 1 sensor 		: TOP
	digital 2 sensor		: LEFT
	digital 3 sensor		: BOTTOM
	digital 4 sensor		: RIGHT
	digital 5 sensor    : TRIGGER
	digital 6 sensor		: VERTICAL Z LIMIT   (Z MAX)
	digital 11 sensor		: DEBUG MODE SWITCH
	digital 12 sensor		: KILL SWITCH

	set up canvas, find width and height

	negative y is "up"
	positive y is "down"
	negative x is "left"
	negative x is "right"




	TODO :
		- bad KILL SWITCH implementation!  SensorValue(killSensor) == 0
*/

int MAX_SPEED = 127;
int HALF_SPEED = 63;
int Z_STEP_SPEED = 40;
int CURSOR_SPEED = 100;
int SQUEEZE_SPEED = 100;
int UNSQUEEZE_WAIT_TIME = 1500; // will be longer in real life
int ZSTEP_DISTANCE = 500; // * 10ms of motor Z_STEP_SPEED  1/49 gear ratio

int cursor_x = 0;
int cursor_y = 0;
int cursor_z = 0;
int canvas_width = 0;
int canvas_height = 0;

bool programComplete = false;
bool debugMode = false; // true will terminate program, (or wait until program is terminated)
												//   then go into debugMode which uses the killswitch to move the cursor into place,
												//   then goes back to debugMode = false

// reset to 0,0,0
void resetCursor(){
	// move y axis all the way to the top

	// detect when the toggle switch is triggered
	while(SensorValue(topSensor) == 0 && SensorValue(killSensor) == 0)		// true IS pressed in
	{
		motor[yMotor] = HALF_SPEED * -1;
	}
	motor[yMotor] = 0;
	// adjustments
	motor[yMotor] = HALF_SPEED;
	//wait1Msec(900);
	while(SensorValue(topSensor) == 1 && SensorValue(killSensor) == 0)		// true IS pressed in
	{
		motor[yMotor] = HALF_SPEED;
	}
	motor[yMotor] = 0;

	if(SensorValue(killSensor) == 1){
		return;
	}

	// move x axis

	// detect when the toggle switch is triggered
	while(SensorValue(leftSensor) == 0 && SensorValue(killSensor) == 0)		// true IS pressed in
	{
		motor[xMotor] = HALF_SPEED * -1;
	}
	motor[xMotor] = 0;
	// adjustments
	motor[xMotor] = HALF_SPEED;
	//wait1Msec(900);
	while(SensorValue(leftSensor) == 1 && SensorValue(killSensor) == 0)		// true IS pressed in
	{
		motor[xMotor] = HALF_SPEED;
	}
	motor[xMotor] = 0;

	// move z axis

	// detect when the toggle switch is triggered
	while(SensorValue(vertSensor) == 0 && SensorValue(killSensor) == 0)		// true IS pressed in
	{
		motor[zMotor] = HALF_SPEED;
	}
	motor[zMotor] = 0;
	wait1Msec(500);
	// adjustments
	motor[zMotor] = HALF_SPEED * -1;
	//wait1Msec(2200); // unpress the limiter
	while(SensorValue(vertSensor) == 1 && SensorValue(killSensor) == 0)		// true IS pressed in
	{
		motor[zMotor] = HALF_SPEED * -1;
	}
	motor[zMotor] = 0;



	// reset values
	cursor_x = 0;
	cursor_y = 0;
	cursor_z = 0;
}

// calibrate, move on x and y axis to find the distance of the canvas
void calibrate(){
	int distance_counter = 0;

	// move y axis all the way to the bottom

	// detect when the toggle switch is triggered
	while(SensorValue(bottomSensor) == 0 && SensorValue(killSensor) == 0 && SensorValue(modeSensor) == 0)		// true IS pressed in
	{
		motor[yMotor] = HALF_SPEED;
		if(distance_counter++ == HALF_SPEED){
			cursor_y++;
			distance_counter = 0;
		}
	}
	motor[yMotor] = 0;
	canvas_height = cursor_y;
	if(SensorValue(killSensor) == 1){
		return;
	}

	// move x axis all the way to the right
	distance_counter = 0;
	// detect when the toggle switch is triggered
	while(SensorValue(rightSensor) == 0 && SensorValue(killSensor) == 0 && SensorValue(modeSensor) == 0)		// true IS pressed in
	{
		motor[xMotor] = HALF_SPEED;
		if(distance_counter++ == HALF_SPEED){
			cursor_x++;
			distance_counter = 0;
		}
	}
	motor[xMotor] = 0;
	canvas_width = cursor_x;

	writeDebugStreamLine("canvas_width : %d", canvas_width);
	writeDebugStreamLine("canvas_height : %d", canvas_height);

	if(SensorValue(modeSensor) == 1){
		debugMode = true;
	}
}



// move from current point to next point
void moveToPoint(int x, int y){
	// find difference of distances
	int x_dist = x-cursor_x;
	int y_dist = y-cursor_y;

	int distance_counter = 0;
	int stepper = 0; // -1 or +1
	int x_traveled = 0;
	int y_traveled = 0;

	// move on the y axis first
	if(y_dist != 0){
		int y_speed = HALF_SPEED;
		if(y_dist < 0){
			y_speed *= -1;
			stepper = -1;
		}else{
			stepper = 1;
		}

		while(cursor_y != y && SensorValue(killSensor) == 0 && SensorValue(modeSensor) == 0)
		{
			motor[yMotor] = y_speed;
			if(distance_counter++ == HALF_SPEED){
				cursor_y += stepper;
				distance_counter = 0;
			}
		}
		motor[yMotor] = 0;
		if(SensorValue(killSensor) == 1){
			return;
		}
	}

	// then move down x axis
	if(x_dist != 0){
		int x_speed = HALF_SPEED;
		if(x_dist < 0){
			x_speed *= -1;
			stepper = -1;
		}else{
			stepper = 1;
		}
		distance_counter = 0;
		while(cursor_x != x && SensorValue(killSensor) == 0 && SensorValue(modeSensor) == 0)
		{
			motor[xMotor] = x_speed;
			if(distance_counter++ == HALF_SPEED){
				cursor_x += stepper;
				distance_counter = 0;
			}
		}
		motor[xMotor] = 0;
		if(SensorValue(killSensor) == 1){
			return;
		}
	}

	if(SensorValue(modeSensor) == 1){
		debugMode = true;
	}

}

/*
	positive step lowers platform
	negative step raises platform
*/
void zStep(int step){
	int direction = -1;
	if(step < 0){
		direction = 1;
		step *= -1;
	}
	for(int i = 0; i < step; i++){
		int stepper = 0;
		motor[zMotor] = Z_STEP_SPEED * direction;
		while(stepper < ZSTEP_DISTANCE && SensorValue(vertSensor) == 0 && SensorValue(killSensor) == 0)		// true IS pressed in
		{
			stepper ++;
			wait1Msec(10);
		}
		motor[zMotor] = 0;
		wait1Msec(500);
	}

}

// apply pixel
// pMotor -> pixel motor -> port 5
void applyPixel(){
	int squeezey = 0;

	motor[pMotor] = SQUEEZE_SPEED;
	while(SensorValue(triggerSensor) == 0 && SensorValue(killSensor) == 0){
		squeezey++;
		wait1Msec(10);
	}
	motor[pMotor] = 0;
	wait1Msec(500);

	motor[pMotor] = -SQUEEZE_SPEED;
	while(squeezey > 0 && SensorValue(killSensor) == 0){
		squeezey--;
		wait1Msec(10);
	}
	//writeDebugStreamLine("squeezey : %d", squeezey);

	motor[pMotor] = 0;
	wait1Msec(UNSQUEEZE_WAIT_TIME);
}



/*************************
 PROGRAM BEGIN
 ************************/

void fillSquare(){
	//writeDebugStreamLine("in fillSquare");
	// start at an origin, and keep coming back to it
		moveToPoint(1500,1500);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		wait1Msec(700);

		// drop first drop
		applyPixel();

		// x:1 y:0
		moveToPoint(2000,1500);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();


		// x:2 y:0
		moveToPoint(2500,1500);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:3 y:0
		moveToPoint(3000,1500);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:4 y:0
		moveToPoint(3500,1500);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// ##
		// x:4 y:1
		moveToPoint(3500,2000);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:3 y:1
		moveToPoint(3000,2000);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:2 y:1
		moveToPoint(2500,2000);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:1 y:1
		moveToPoint(2000,2000);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:0 y:1
		moveToPoint(1500,2000);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// ##
		// x:0 y:2
		moveToPoint(1500,2500);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:1 y:2
		moveToPoint(2000,2500);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:2 y:2
		moveToPoint(2500,2500);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:3 y:2
		moveToPoint(3000,2500);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:4 y:2
		moveToPoint(3500,2500);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();


		// ##
		// x:4 y:3
		moveToPoint(3500,3000);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:3 y:3
		moveToPoint(3000,3000);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:2 y:3
		moveToPoint(2500,3000);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:1 y:3
		moveToPoint(2000,3000);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:0 y:3
		moveToPoint(1500,3000);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();


		// ##
		// x:0 y:4
		moveToPoint(1500,3500);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:1 y:4
		moveToPoint(2000,3500);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:2 y:4
		moveToPoint(2500,3500);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:3 y:4
		moveToPoint(3000,3500);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();

		// x:4 y:4
		moveToPoint(3500,3500);
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		applyPixel();
}



void printer3d(){
		// to start, this will break if the sensor is already touching
		resetCursor();

		wait1Msec(500);

		// get canvas information
		/* disable for now
		calibrate();
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		// ok move back
		resetCursor();
		wait1Msec(500);*/

		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		wait1Msec(1500);


		// z = 0
		fillSquare();
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		wait1Msec(1000);
		zStep(1);

		// z = 1
		fillSquare();
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		wait1Msec(1000);
		zStep(1);

		// z = 2
		fillSquare();
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		wait1Msec(1000);
		zStep(1);

		// z = 3
		fillSquare();
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		wait1Msec(1000);
		zStep(1);

		// z = 4
		fillSquare();
		if(debugMode || SensorValue(killSensor) == 1){
			return;
		}
		wait1Msec(1000);
		zStep(1);


		// DON'T DO THIS
		// it'll smash the 3d object
		// resetCursor();
		// just move z step down a few
		zStep(20);
		programComplete = true;
}


// Y X Z, -Y, -X, -Z
void debugCursor(){
	while(SensorValue(killSensor) == 0)		// true IS pressed in
	{
		motor[yMotor] = MAX_SPEED ;
	}
	motor[yMotor] = 0;
	wait1Msec(2000);
	while(SensorValue(killSensor) == 0)		// true IS pressed in
	{
		motor[xMotor] = MAX_SPEED ;
	}
	motor[xMotor] = 0;
	wait1Msec(3500);

	while(SensorValue(killSensor) == 0)		// true IS pressed in
	{
		motor[yMotor] = MAX_SPEED *-1 ;
	}
	motor[yMotor] = 0;
	wait1Msec(2000);

	while(SensorValue(killSensor) == 0)		// true IS pressed in
	{
		motor[xMotor] = MAX_SPEED *-1;
	}
	motor[xMotor] = 0;
	wait1Msec(2000);

	debugMode = false;
}

task main()
{
	wait1Msec(2000);						// Robot waits for 2000 milliseconds before executing program


	while(!programComplete){

		if(debugMode){
			// debug stuff
			debugCursor();
		}else{
			// actual program
			printer3d();
		}

		//while(SensorValue(killSensor) == 0){ // kill switch
		//}

	}
}

// Program ends, and the robot stops
