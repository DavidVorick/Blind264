//////////////////////////////////////////////////////////////////
// Blind 264 by Taek                                            //
// May not compile properly on all machines                     //
// Full Code Revision 2                                         //
// Code is intended to be easy to read, understand, and modify  //
//////////////////////////////////////////////////////////////////

//to-do list:

//4. do the test for aq-mode -|-|-> use bitrate as a guide
//5. guess aq-strength -|-|-|-|-|-> I guess that it's okay to use ssim as a guide
//6. guess psy-rd |-|-|-|-|-|-|-|-> Use the other settings as a guide .,.,.,., experimental! (higher crf = lower psy, higher aq = lower psy, aq 2 = lower psy, higher qcomp = higher psy, higher bitrate = higher psy)
//9. each test in findHardValues should have its own .avs
//a. potentially, you can look at the streamed kb/s output to determine where each encode has more or less bits.

//mode 1: do all the stuff that doesn't need any testing
//mode 2: do mode's 1 and then use x264 to find the 6 important values
//note: some parts of mode 2 are still in the 'theory' stage and might be excluded if the theories don't pan out
//mode 3: do a bunch of range testing using given settings
//given the current code structure, picking different modes doesn't actually do anything

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//configuration variables
char configloc[500] = "C:/b264/config.txt"; //configuration for the actual blin264. These variable affect the operation of b264.exe only
char x264loc[500] = "C:\\b264\\x264.exe"; //location of x264.exe
char avs2yuvloc[500] = "C:\\b264\\avs2yuv.exe"; //location of avs2yuv.exe
char logdir[500] = "C:/b264/logs"; //the filepath to store all log files
char x264defaults[500] = "C:/b264/x264_defaults.txt"; // default variables for x264, these variables affect primarily the operation of x264.exe. Using multiple default files can allow one to have presets.
char x264current[500] = "C:/b264/x264_current.txt"; //so that blind264 can be called multiple times without forgetting the previous values it found.
char initialavs[500] = "C:/A/mup/mup.avs"; //location of the .avs file that will be used to preprocess the video (cropping, deinterlacing, detelcining)
float bframethreshold = 0.5; //used in the bframes algorithm. Higher values will result in less bframes. Consult algorithm for more info
unsigned int psize = 480; //480p? 576p? what definition should the source video be resized to?
unsigned int modetype = 1; //affects which functions within b264 will be called.

//x264 settings
int deblock_alpha = -3;
int deblock_beta = -3;
unsigned int aqmode = 1;
float aqs = 1.0; //aq-strength
float psyr = 0.9; //psy-rd
float psyt = 0.0; //psy-trellis
float qcomp = 0.7;
char me[5] = "umh";
unsigned int merange = 16;
unsigned int ref = 4;
unsigned int bframes = 16;
float crf = 16.5;

//running x264. There should only be 1 x264 process at a time, which lets us cluster variables and use funtions
FILE* x264Process;
FILE* x264log;
char x264Command[1200];
char x264logName[800];

//general variables that are given global scope to make the code easier to work with
char readLineFromInput[1500]; //makes using findSequence easier
int sequenceFound; //a return value for findSequence. It's a bit of a hack because it was late in being implimented =/
FILE* b264log; //outputs all info put to command line, along with lots of debugging info
char nextPartOfLog[2500]; //variable to do text manipulation and then ouput to the log

//general information about the source and encode videos
int sourceWidth;
int sourceHeight;
int encodeWidth;
int encodeHeight;
float sourceAspectRatio;
float encodeAspectRatio;
float framerate;
int framecount;

//proper use of findSequence:
//1. put the desired line of text in readLineFromInput
//2. since c can be awkward sometimes, you have to also tell findSequence how long the findme string is
//2. findSequence will return all characters in the readLineFromInput occuring after the first instance of findme[]
//3. It's a bit of a hack because I don't really know how to do string manipulation
void findSequence(char findme[20], int findmeLength) {
  int i = 0;
  int j = 0;
  sequenceFound = 0;
  while(i<1400 && sequenceFound == 0 && readLineFromInput[i] != '\0' && readLineFromInput[i] != '\n') {
    j = 0;
	while(j<findmeLength && readLineFromInput[i+j] == findme[j] && sequenceFound == 0) {
	  j++;
	  if(j == findmeLength) {
	    sequenceFound = 1;
		j = 0;
		i += findmeLength;
		while(i<1400 && readLineFromInput[i] != '\0' && readLineFromInput[i] != '\n') {
		  readLineFromInput[j] = readLineFromInput[i];
		  i++;
		  j++;
		}
		readLineFromInput[j] = '\0';
	  }
	}
	i++;
  }
}

int readConfigFiles(int argc, char* argv[]) {
  FILE* b264config;
  
  //The first thing that we check for is if the location of the config file has been changed through the cli.
  int i = 1;
  while(i < argc && i < 100) {	
	if(strcmp(argv[i],"--conf") == 0 || strcmp(argv[i],"--config") == 0 || strcmp(argv[i],"--c") == 0) {
	  snprintf(configloc, sizeof(configloc), "%s", argv[i+1]);
	  break;
	}
	i++;
  }
  
  b264config = fopen(configloc, "r");
  if(b264config == NULL) {
    printf("Could not open config file");
	return -1;
  }
  //Read from config.txt and set the variables to the proper values
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--x264loc ", 10);
  snprintf(x264loc, sizeof(x264loc), "%s", readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--avs2yuvloc ", 13);
  snprintf(avs2yuvloc, sizeof(avs2yuvloc), "%s", readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--logdir ", 9);
  snprintf(logdir, sizeof(logdir), "%s", readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--x264def ", 10);
  snprintf(x264defaults, sizeof(x264defaults), "%s", readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--x264cur ", 10);
  snprintf(x264current, sizeof(x264current), "%s", readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--avs ", 6);
  snprintf(initialavs, sizeof(initialavs), "%s", readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--bft ", 6);
  bframethreshold = atof(readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--psize ", 7);
  psize = atoi(readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--mode ", 7);
  modetype = atoi(readLineFromInput);
  
  //the next part of the file is potentially read from a different .txt, which is potentially set in the cli, so we have to parse the cli before continuing.
  i = 1;
  while(i < argc) {	
	if(strcmp(argv[i],"--x264cur") == 0) {
	  snprintf(x264current, sizeof(x264current), "%s", argv[i+1]);
	  break;
	}
	i++;
  }
  
  //this is the statement that requires the quick parse of command line arguments
  if(modetype == 3)
    b264config = fopen(x264defaults, "r");
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--deblock ", 10);
  deblock_alpha = atoi(readLineFromInput);
  findSequence(":", 1);
  deblock_beta = atoi(readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--aq-mode ", 10);
  aqmode = atoi(readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--aq-strength ", 14);
  aqs = atof(readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--psy-rd ", 9);
  psyr = atof(readLineFromInput);
  findSequence(":", 1);
  psyt = atof(readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--qcomp ", 8);
  qcomp = atof(readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--me ", 5);
  snprintf(me, sizeof(me), "%s", readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--merange ", 10);
  merange = atoi(readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--ref ", 6);
  ref = atoi(readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--bframes ", 10);
  bframes = atoi(readLineFromInput);
  
  fgets(readLineFromInput, sizeof(readLineFromInput), b264config);
  findSequence("--crf ", 6);
  crf = atof(readLineFromInput);

  fclose(b264config);  
  return 1;
}

int readCliArgs(int argc, char* argv[]) {
  int i = 1;
  while(i < argc) {	
	if(strcmp(argv[i],"--x264loc") == 0 || strcmp(argv[i],"--x264") == 0) {
	  snprintf(x264loc, sizeof(x264loc), "%s", argv[i+1]);
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--avs2yuv") == 0 || strcmp(argv[i],"--avs2yuvloc") == 0 || strcmp(argv[i],"--al") == 0) {
	  snprintf(avs2yuvloc, sizeof(avs2yuvloc), "%s", argv[i+1]);
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--dir") == 0 || strcmp(argv[i],"--logdir") == 0 || strcmp(argv[i],"--ld") == 0) {
	  snprintf(logdir, sizeof(logdir), "%s", argv[i+1]);
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--x264def") == 0 || strcmp(argv[i],"--xd") == 0) {
	  snprintf(x264defaults, sizeof(x264defaults), "%s", argv[i+1]);
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--x264cur") == 0 || strcmp(argv[i],"--xc") == 0) {
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--avs") == 0 || strcmp(argv[i],"--a") == 0) {
	  snprintf(initialavs, sizeof(initialavs), "%s", argv[i+1]);
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--bft") == 0) {
	  bframethreshold = atof(argv[i+1]);
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--psize") == 0 || strcmp(argv[i],"--p") == 0) {
	  psize = atoi(argv[i+1]);
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--mode") == 0 || strcmp(argv[i],"--m") == 0) {
	  modetype = atoi(argv[i+1]);
	  i += 2;
	  continue;
	}
		
	if(strcmp(argv[i],"--deblock") == 0) {
	  deblock_alpha = atoi(argv[i+1]);
      snprintf(readLineFromInput, sizeof(readLineFromInput), "%s", argv[i+1]);
	  findSequence(":", 1);
      deblock_beta = atoi(readLineFromInput);
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--aq-mode") == 0) {
	  aqmode = atoi(argv[i+1]);
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--aq-strength") == 0) {
	  aqs = atof(argv[i+1]);
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--psy-rd") == 0) {
      psyr = atof(argv[i+1]);
      snprintf(readLineFromInput, sizeof(readLineFromInput), "%s", argv[i+1]);
	  findSequence(":", 1);
      psyt = atof(readLineFromInput);
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--qcomp") == 0) {
	  qcomp = atof(argv[i+1]);
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--me") == 0) {
	  snprintf(me, sizeof(me), "%s", argv[i+1]);;
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--merange") == 0) {
	  merange = atoi(argv[i+1]);
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--ref") == 0) {
	  ref = atoi(argv[i+1]);
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--bframes") == 0) {
	  bframes = atoi(argv[i+1]);
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--crf") == 0) {
	  crf = atof(argv[i+1]);
	  i += 2;
	  continue;
	}
	
	if(strcmp(argv[i],"--conf") == 0) {
	  i += 2;
	  continue;
	}
		
	printf("\n\nInvalid Parameter %u: %s \n", i, argv[i]);
	return -1;
  }  
  return 1;
}

//findEasyValues() is called by mode 1 and mode 2
//findEasyValues() is called such because it doesn't use x264, and finishes in seconds
//findEasyValues() is mainly to save you manual work, finding the following: 
//the original resolution of the source video
//the framerate of the source video
//the framecount of the source video
//the aspect ratio of the source video
//the most accurate resize values given --psize
//the maximum number of compliant ref frames
int findEasyValues() {
  //call avs2yuv and get basic information about the video
  char avs2yuvCommand[500];
  snprintf(avs2yuvCommand, sizeof(avs2yuvCommand), "%s -frames 1 %s NUL 2>&1", avs2yuvloc, initialavs);
  
  //announce that avs2yuv is being called (print the announcement to both, print the acutal command used only to the log)
  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\n\nCalling avs2yuv");
  printf(nextPartOfLog);
  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "%s with command: %s", nextPartOfLog, avs2yuvCommand);
  fputs(nextPartOfLog, b264log);
  
  //call avs2yuv, retrieve output and close the process
  FILE* avs2yuvProcess = popen(avs2yuvCommand, "r");
  fgets(readLineFromInput, 900, avs2yuvProcess);
  pclose(avs2yuvProcess);
  
  //print the avs2yuv output to both the command line and the log
  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "  \noutput: %s", readLineFromInput);
  fputs("avs2yuv ", b264log);
  fputs(nextPartOfLog, b264log);
  printf(nextPartOfLog);
  
  //parse the input to get resolution, framerate, and number of frames
  findSequence(".avs: ", 6);
  sourceWidth = atoi(readLineFromInput);
  findSequence("x", 1);
  sourceHeight = atoi(readLineFromInput);
  findSequence(", ", 2);
  int numerator = atoi(readLineFromInput);
  findSequence("/", 1);
  int denominator = atoi(readLineFromInput);
  framerate = (float)numerator/(float)denominator;
  findSequence("fps, ", 5);
  framecount = atoi(readLineFromInput);
  
  sourceAspectRatio = (float)sourceWidth/(float)sourceHeight;

  if(sourceAspectRatio >= 1.77777) {
    encodeWidth = ceil((float)psize*1.777777);
	if(encodeWidth%2 != 0)
	  encodeWidth--;
	  
    encodeHeight = ceil((float)encodeWidth/sourceAspectRatio);
	if(encodeHeight%2 != 0)
	  encodeHeight--;
	  
  } else {
    encodeHeight = psize;
	if(encodeHeight%2 != 0)
	  psize--;
	  
    encodeWidth = ceil((float)encodeHeight*sourceAspectRatio);
	if(encodeHeight%2 != 0)
	  encodeHeight--;
  }
  
  encodeAspectRatio = (float)encodeWidth/(float)encodeHeight;
  
  //print the parsed information to b264log to make sure everything was parsed properly. There's no need to use the cli because the avs2yuv output was already displayed.
  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\nsourceAspectRatio: %g\nencodeAspectRatio: %g\nSource Dimensions: %ux%u\nEncode Dimensions: %ux%u\nFramerate: %g\nFramecount: %u", sourceAspectRatio, encodeAspectRatio, sourceWidth, sourceHeight, encodeWidth, encodeHeight, framerate, framecount);
  fputs(nextPartOfLog, b264log);
  
  //print the usful information: source aspect ratio, encode dimensions, encode aspect ratio
  printf("\nEncode Dimentions: %ux%u\nSource Aspect Ratio: %g\nEncode Aspect Ratio: %g", encodeWidth, encodeHeight, sourceAspectRatio, encodeAspectRatio);
  
  //a strightforward calculation to get the maximum number of compliant ref frames
  ref = 8388608;
  ref /= (encodeHeight*encodeWidth);
  if(ref > 16)
    ref = 16;
  
  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\n\nRef Frames: %u", ref);
  fputs(nextPartOfLog, b264log);
  printf(nextPartOfLog);
  
  return 1;
}

int findRoughValues() {
  //////////////////////
  //START BFRAMES TEST//
  //////////////////////
  //state that the bframes test is starting
  //this is so that in the logs (and while the command line is running), it's clear what messages belong to what stage of the process
  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\n\nStarting bframes test\n");
  fputs(nextPartOfLog, b264log);
  printf(nextPartOfLog);
  
  //open the avs file and store all of its contents into a string
  FILE* initialAvsReader = fopen(initialavs, "r");
  char changingAvs[1400]; //This is to prevent characters like '\0' from sneaking into the avs file prematurely
  snprintf(changingAvs, sizeof(changingAvs), "");  //sometimes changingAvs was not empty by default, so this line was added to clear changingAvs
  while(fgets(readLineFromInput, sizeof(readLineFromInput), initialAvsReader) != NULL)
    snprintf(changingAvs, sizeof(changingAvs), "%s%s", changingAvs, readLineFromInput);
  
  //pick out the sample space for the bframes test
  //this is only the rough values; a small number of frames is chosen
  //skip the first and last 9% because it's usually credits
  //every 9% of the film is sampled, which will hopefully grab cuts from at least 1/2 of the scenes.
  //400 frames from each cut are grabbed; compromise to try and get a good idea of what the scene is like
  int startCredits = framecount*.09;
  int endCredits = framecount*.91;
  int selectEvery = framecount*.085;
  snprintf(changingAvs, sizeof(changingAvs), "%s\ntrim(%u,%u)\nSelectRangeEvery(%u,400,0)\nSpline36Resize(%u,%u)", changingAvs, startCredits, endCredits, selectEvery, encodeWidth, encodeHeight);
  
  //create the variable that will name the avs file to be used while doing the bframes test
  char roughAvsName[800];
  snprintf(roughAvsName, sizeof(roughAvsName), "%s/roughTesting.avs", logdir);
  
  //write the avs file that will be used for the bframes test
  FILE* roughAvs = fopen(roughAvsName, "w");
  fputs(changingAvs, roughAvs);
  fclose(roughAvs);
  
  //write the command to run the x264 bframes test
  //the bframes test uses ref 1 and subme 2 and --no-deblock and --trellis 0 and --partitions none because these settings actually do not affect how bframes are placed. This allows the bframes test to run significantly faster without changing the end result.
  snprintf(x264Command, sizeof(x264Command), "%s %s -o - | %s --preset placebo --partitions none --no-deblock --rc-lookahead 250 --subme 2 --trellis 0 --aq-mode %u --aq-strength %g --qcomp %g --me %s --merange %u --ref 1 --bframes 16 --crf %g -o %s/BframesTest.mkv - --demuxer y4m NUL 2>&1", avs2yuvloc, roughAvsName, x264loc, aqmode, aqs, qcomp, me, merange, crf, logdir);
  
  //write the command being called to the log, don't bother putting it to cli output
  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\nBframes Command: %s", x264Command);
  fputs(nextPartOfLog, b264log);

  //print all x264 output to it's own log file, do not crowd out b264log with the output
  snprintf(x264logName, sizeof(x264logName), "%s/bframestest.log", logdir);
  x264log = fopen(x264logName, "w");
  
  //call x264
  x264Process = popen(x264Command, "r");
  
  //clears sequenceFound, which is probably 1. This is only so that the loop starts in the first place.
  sequenceFound = 0;
  while(sequenceFound == 0) {  //sequenceFound will be 0 if findSequence fails to find the string that is being looked for
    //read the next line from x264output, print it to the log
	//with the exception of all the lines that look like this: "24 frames: 0.42 fps, 21868.67 kb/s  "
	//because there are usually 100s to 1000s of those lines and they aren't useful in everyday use
	//new features can always be implimented if a use is found
	fgets(readLineFromInput, sizeof(readLineFromInput), x264Process);
	if(readLineFromInput[0] < 48 || readLineFromInput[0] > 57)
	  fputs(readLineFromInput, x264log);
	
	//search for the line of code with the statistics about consecutive bframes
    findSequence("B-frames: ", 10);
	if(sequenceFound == 1) {
	  //print the consecutive bframes information to the b264 log, for context, and also because you might want that info more often than you want to see the whole log
	  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\nx264 Consecutive Bframes: %s", readLineFromInput);
	  fputs(nextPartOfLog, b264log);
	  
	  //the following code goes through the consecutive bframes and
	  //returns the highest bframes value such that
	  //each bframes added is worth at least "bframethreshold" more percent
	  //example: 10, 5, 1, 1, 5
	  //if "bframethreshold" is 2, 10 will be added b/c it's worth 2, 5 will be added because it's worth 2, then 1, 1, 5 will be added b/c the whole thing is worth 7 (more than 2 per bframe added)
	  //example2: 10, 5, 0, 0, 5
	  //10 and 5 will be added, but 005 will not be added, because the whole thing is worth 5, which is less than 2 per bframe added
	  int i = 0;
	  float bRequirement = bframethreshold;
	  float currentBframeValue;
	  while(i <= 16) {
	    //x264 has output with variable amounts of spaces between the numbers
		//so this code removes all of them, but only the spaces and %s
	    while(readLineFromInput[0] == ' ' || readLineFromInput[0] == '%')
	      findSequence(" ", 1);
		
		currentBframeValue = atof(readLineFromInput);
		
		if(bRequirement < currentBframeValue) {
		  bframes = i;
		  bRequirement = bframethreshold;
		} else {
		  bRequirement += bframethreshold;
		  bRequirement -= currentBframeValue;
		}
		i++;
	  }
	}
  }
  
  //this loop is simply to get the rest of the x264 output to finish bframestest.log
  sequenceFound = 0;
  while(sequenceFound == 0) {
    fgets(readLineFromInput, sizeof(readLineFromInput), x264Process);
	fputs(readLineFromInput, x264log);
	findSequence("encoded", 7);
  }
  
  //x264Process has terminated and all the output has been stored in the log
  fclose(x264log);
  pclose(x264Process);
  
  //output the bframes value picked to both the cli and the log
  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\nBframes Value Picked: %u\n", bframes);
  printf(nextPartOfLog);
  fputs(nextPartOfLog, b264log);  
  
  ///////////////////
  //START CRF TESTS//
  ///////////////////
  int crfIterations = 1;
  float db;
  float bitrate;
  while(crfIterations <= 3) {
    //let the user know that the program has proceeded to the next stage of testing
    printf("\nStarting Crf Test %u:\nCurrent Crf Value: %g\n", crfIterations, crf);
	
	//the x264 command; uses the same avs as the bframes testing
    snprintf(x264Command, sizeof(x264Command), "%s %s -o - | %s --preset placebo --rc-lookahead 250 --subme 11 --deblock%i:%i --aq-mode %u --aq-strength %g --qcomp %g --me %s --merange %u --ref %u --bframes %u --crf %g --no-psy --ssim -o %s/crftest%u.mkv - --demuxer y4m NUL 2>&1", avs2yuvloc, roughAvsName, x264loc, deblock_alpha, deblock_beta, aqmode, aqs, qcomp, me, merange, ref, bframes, crf, logdir, crfIterations);
    
	//print to b264log the command and context information
    snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\nStarting Crf Test %u\nCrf Test Command: %s", crfIterations, x264Command);
	fputs(nextPartOfLog, b264log);
	
	//create the x264 log so that one can start printing to it.
	snprintf(x264logName, sizeof(x264logName), "%s/crftest%u.log", logdir, crfIterations);
    x264Process = popen(x264Command, "r");
	x264log = fopen(x264logName, "w");
	
	//clear sequenceFound to enter the loop, and start collecting log information
	//collection in this loop stops when the db information is found
    sequenceFound = 0;
    while(sequenceFound == 0) {
      fgets(readLineFromInput, sizeof(readLineFromInput), x264Process);
	  fputs(readLineFromInput, x264log);
      findSequence("SSIM Mean", 9);
	
  	  if(sequenceFound == 1) {
        findSequence("(", 1);
	    db = atof(readLineFromInput);
   	  }
    }
	
	//this loop looks for the encode bitrate
	sequenceFound = 0;
    while(sequenceFound == 0) {
      fgets(readLineFromInput, sizeof(readLineFromInput), x264Process);
	  fputs(readLineFromInput, x264log);
      findSequence("kb/s:", 5);
	  
	  if(sequenceFound == 1)
        bitrate = atof(readLineFromInput);
    }
	
	//print the rest of the output to the log
	sequenceFound = 0;
    while(sequenceFound == 0) {
      fgets(readLineFromInput, sizeof(readLineFromInput), x264Process);
	  fputs(readLineFromInput, x264log);
	  findSequence("encoded", 7);
    }
	
	//we are looking for a specific db relative to the (bitsPerSecond/(width*height*framesPerSecond)), which is the same as Bits/(Pixel*frame), which is shortened to bppf
	//a good bppf is usually between .15 and .35, though values outside of this are not uncommon b/c videos vary drastically in compressability
	//if the bppf is high and the db is high, that probably means the video is more compressible, and so bits can be removed. If the bppf is low and the db is low, that probably means that the video is less compressible and needs more bits.
	//the function is linear such that as a video has more bits, the target db for that video goes down.
	//the equation, which I would typically render in 1 line of code, has been changed to 5 lines of code so that it's more clear how everything works.
	float dbAtZeroBitrate = 21.6;
	float dbReductionPerIncreaseInBPPF = 16.6;
	float adjustmentConstant = 1.3;
	float bppf = (bitrate*framerate)/(encodeWidth*encodeHeight);//I discovered at one point that bppf equals this equation, but I have since forgotten why and how
	float desiredDBatCurrentBitrate = dbAtZeroBitrate - dbReductionPerIncreaseInBPPF*bppf;
	float crfAdjustment = (db-desiredDBatCurrentBitrate)/adjustmentConstant; //adjust crf in the correct direction to move closer to desired db. the adjustmentConstant is arbitrary and poorly understood. 1.3 simply seems to work better than 1 (otherwise the crf tends to overshoot the goal)
	
	//print to the log information about how the crfAdjustment equation functioned.
	//this is to help refine the equation, because it's not really well understood and probably not optimal
	snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\ndbAtZeroBitrate: %g\ndbReductionPerIncreaseInBPPF: %g\nadjustmentConstant: %g\nbppf: %g\ndb: %g\ndesiredDBatCurrentBitrate: %g\ncrfAdjustment: %g\nold crf: %g\nnew crf: %g\n", dbAtZeroBitrate, dbReductionPerIncreaseInBPPF, adjustmentConstant, bppf, db, desiredDBatCurrentBitrate, crfAdjustment, crf, crf+crfAdjustment);
	fputs(nextPartOfLog, b264log);
	
	//if the variance is less than .4, we are done, otherwise finishing adjusting
	if(crfAdjustment > 0) {
	  if(crfAdjustment < .4)
	    crfIterations = 3; //this will cause the loop to terminate
	  crf += crfAdjustment;
	} else {
	  if(crfAdjustment > -.4)
	    crfIterations = 3; //this will cause the loop to terminate
	  crf += crfAdjustment;
	}
	crfIterations++;
  }
  
  ///////////////////////
  //START QCOMP TESTING//
  ///////////////////////
  //this section is under commented because it was just written and I was tired
  //print the final crf value both to cli and to log
  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\nFinal Crf Value: %g", crf);
  fputs(nextPartOfLog, b264log);
  printf(nextPartOfLog);
  
  //this loop tries to find the appropriate qcomp value
  int qcompIteration = 1;
  int newdb = db;
  int qcompState = 0; //0 = untested, 1 = moving in a + direction, -1 = moving in a - direction, 2 = have been moving in a positive direction, -2 = have been moving in a negative direction
  while(qcompIteration <= 5) {
    //right now we are just going to try and approximate the bitrate by moving the crf an appropriate amount. This will cause problems if the crf approximation is off. In the future, algorithms can be added/changed so that the bitrate is closer.
	snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\nStarting qcomp test %u", qcompIteration);
    fputs(nextPartOfLog, b264log);
    printf(nextPartOfLog);
	
	//move qcomp .05, then adjust crf by .1
	if(qcompState == 0) {
	  qcomp += .05;
	  crf += .1;
	  qcompState = 1;
	} else {
	  if(newdb > db && qcompState > 0) {
	    qcomp += .05;
		crf += .1;
		qcompState = 2;
	  } else if(newdb > db && qcompState < 0) {
	    qcomp -= .05;
		crf -= .1;
		qcompState = -2;
	  } else if(newdb < db && qcompState == 1) {
	    qcomp -= .1;
		crf -= .2;
		qcompState = -1;
	  } else if(newdb < db && qcompState < 0) {
	    qcomp += .05;
		crf += .1;
		break;
	  } else if(newdb < db && qcompState > 0) {
	    qcomp -= .05;
		crf -= .1;
		break;
	  }
	}
	
    snprintf(x264Command, sizeof(x264Command), "%s %s -o - | %s --preset placebo --rc-lookahead 250 --subme 11 --deblock%i:%i --aq-mode %u --aq-strength %g --qcomp %g --me %s --merange %u --ref %u --bframes %u --crf %g --psy-rd %g:%g --ssim -o %s/qcomptest%u.mkv - --demuxer y4m NUL 2>&1", avs2yuvloc, roughAvsName, x264loc, deblock_alpha, deblock_beta, aqmode, aqs, qcomp, me, merange, ref, bframes, crf, psyr, psyt, logdir, qcompIterations);
	
	//print to b264log the command and context information
    snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\nStarting Qcomp Test %u\nQcomp Test Command: %s", qcompIterations, x264Command);
	fputs(nextPartOfLog, b264log);
	
	//create the x264 log so that one can start printing to it.
	snprintf(x264logName, sizeof(x264logName), "%s/qcomptest%u.log", logdir, qcompIterations);
    x264Process = popen(x264Command, "r");
	x264log = fopen(x264logName, "w");
	
	//clear sequenceFound to enter the loop, and start collecting log information
	//collection in this loop stops when the db information is found
    sequenceFound = 0;
    while(sequenceFound == 0) {
      fgets(readLineFromInput, sizeof(readLineFromInput), x264Process);
	  fputs(readLineFromInput, x264log);
      findSequence("SSIM Mean", 9);
	
  	  if(sequenceFound == 1) {
        findSequence("(", 1);
	    newdb = atof(readLineFromInput);
   	  }
    }
	
	//print the rest of the output to the log
	sequenceFound = 0;
    while(sequenceFound == 0) {
      fgets(readLineFromInput, sizeof(readLineFromInput), x264Process);
	  fputs(readLineFromInput, x264log);
	  findSequence("encoded", 7);
    }
  }
}

int main(int argci, char* argvi[])
{
  printf("\n==================================================\nProgram Start\n");
  //reading the config file is the first thing, because the config file has preference over the program defaults (set when setting the global variables), but the cli has precedent over the defaults file.
  if(readConfigFiles(argci,argvi) != 1)
    return -1; 
  
  //create the .txt that will contain all b264 output and debugging information
  mkdir(logdir);
  char b264logName[600];
  snprintf(b264logName, sizeof(b264logName), "%s/b264log.txt", logdir);
  b264log = fopen(b264logName, "w");

  //print out how the program interpreted the config file. This is a debugging statement
  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "Configuration as read from the config files (cli for --config and --x264cur included):\nConfigloc: %s\nx264loc: %s\navs2yuvloc: %s\nlogdir: %s\nx264def: %s\nx264cur: %s\navs: %s\nbft: %g\npsize: %u\nmode: %u\ndeblock: %i:%i\naq: %i:%g\npsy-rd: %g:%g\nqcomp: %g\nme: %s:%u\nref: %u\nbframes: %u\ncrf: %g", configloc, x264loc, avs2yuvloc, logdir, x264defaults, x264current, initialavs, bframethreshold, psize, modetype, deblock_alpha, deblock_beta, aqmode, aqs, psyr, psyt, qcomp, me, merange, ref, bframes, crf);
  fputs(nextPartOfLog, b264log);

  //read these second because they have highest priority
  if(readCliArgs(argci, argvi) != 1)
    return -1; 
	
  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\n\nConfiguration after reading cli:\nConfigloc: %s\nx264loc: %s\navs2yuvloc: %s\nlogdir: %s\nx264def: %s\nx264cur: %s\navs: %s\nbft: %g\npsize: %u\nmode: %u\ndeblock: %i:%i\naq: %i:%g\npsy-rd: %g:%g\nqcomp: %g\nme: %s:%u\nref: %u\nbframes: %u\ncrf: %g", configloc, x264loc, avs2yuvloc, logdir, x264defaults, x264current, initialavs, bframethreshold, psize, modetype, deblock_alpha, deblock_beta, aqmode, aqs, psyr, psyt, qcomp, me, merange, ref, bframes, crf);
  fputs(nextPartOfLog, b264log);
  
  //find out all the stuff that doesn't require x264, and therefore runs quickly
  //if the file isn't indexed, it might still take a while
  if(findEasyValues() != 1)
    return -1;
    
  //use x264 and the output to get a rough idea of the best settings
  //should run comparatively quickly (encoding a total of maybe 40% of the movie)
  if(findRoughValues() != 1)
    return -1;
  
  fclose(b264log);
  printf( "\n\nProgram End \n==================================================\n" );
}
