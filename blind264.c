//////////////////////////////////////////////////////////////////
// Blind 264 by Taek                                            //
// May not compile properly on all machines                     //
// Full Code Revision 2                                         //
// Code is intended to be easy to read, understand, and modify  //
//////////////////////////////////////////////////////////////////

//to-do list:

//1. Impliment aq-mode. This is mostly a blind guess :(, but it seems that when the bitrates are close, aq-mode 2 is superior, otherwise aq-mode 1 is superior.
     //actually, aq-mode 1 seems to be superior very often. Perhaps aq-2 is inferior the same way mbtree is inferior.
//2. Impliment aq-strength. My instinctual guess is that higher bitrates suggest the need for a lower aq-strength, and vice-versa. 
     //new evidence actually shows that this is probably a bad idea.
//3. Attempt psy-rd. Nobody has any idea how to do this, but some stabs in the dark are listed below
     //higher crf = lower psy, higher aq = lower psy, aq 2 = lower psy, higher qcomp = higher psy, higher bitrate = higher psy
//4. there's another algorithm for guessing qcomp that seems to be able to figure out the correct value in just 2 tests (.7 and .75, and then figures out the correct value based on the difference in the db), but this has only been tested on a few sources and so isn't implimented because it might not actually be consistent.
//5. Add 'mode 3', the tool that creates a bunch of encodes at similar bitrates across a variety of values.

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

//findEasyValues() is called such because it doesn't use x264, and finishes in seconds
//findEasyValues() finds
//the resolution of the encode given the desired --psize, as well as the resulting aspect ratios of both the source and encode
//and the maximum number of compliant ref frames
//for findRoughValues() it also finds the framerate
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
  snprintf(x264Command, sizeof(x264Command), "%s %s -o - | %s --preset placebo --no-mbtree --partitions none --no-deblock --rc-lookahead 250 --subme 2 --trellis 0 --aq-mode %u --aq-strength %g --qcomp %g --me %s --merange %u --ref 1 --bframes 16 --crf %g -o %s/BframesTest.mkv - --demuxer y4m NUL 2>&1", avs2yuvloc, roughAvsName, x264loc, aqmode, aqs, qcomp, me, merange, crf, logdir);
  
  //write the command being called to the log, don't bother putting it to cli output
  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\nBframes Command: %s", x264Command);
  fputs(nextPartOfLog, b264log);

  //print all x264 output to it's own log file, and start the process
  snprintf(x264logName, sizeof(x264logName), "%s/bframestest.log", logdir);
  x264log = fopen(x264logName, "w");
  x264Process = popen(x264Command, "r");
  
  //clears sequenceFound, which is probably 1. This is only so that the loop starts in the first place.
  //the first 2 while loops keep x264 from outputting 100s of current bitrate statements. Comment them out to see what I mean..
  sequenceFound = 0;
  while(sequenceFound == 0) {
	fgets(readLineFromInput, sizeof(readLineFromInput), x264Process);
    fputs(readLineFromInput, x264log);
	findSequence("profile", 7);
  } 
  
  sequenceFound = 0;
  while(sequenceFound == 0) {
    fgets(readLineFromInput, sizeof(readLineFromInput), x264Process);
	findSequence("x264", 4);
  }
  
  fputs("\nx264", x264log);
  fputs(readLineFromInput, x264log);
  fputs("\n", x264log);
  
  sequenceFound = 0;
  while(sequenceFound == 0) {  
	fgets(readLineFromInput, sizeof(readLineFromInput), x264Process);
	fputs(readLineFromInput, x264log);
	
	//search for the line of code with the statistics about consecutive bframes
    findSequence("B-frames: ", 10);
	if(sequenceFound == 1) {
	  //print the consecutive bframes information to the b264 log, for context, and also because you might want that info more often than you want to see the whole log
	  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\nx264 Consecutive Bframes: %s", readLineFromInput);
	  fputs(nextPartOfLog, b264log);
	  
	  //bframes algorithm described in readme.txt
	  int i = 0;
	  float bRequirement = bframethreshold;
	  float currentBframeValue;
	  while(i <= 16) {
	    //x264 has output with variable amounts of spaces between the numbers
		//so this code removes all of them, but only the spaces and '%'s
	    while(readLineFromInput[0] == ' ')
		  findSequence(" ", 1);		    
			
		currentBframeValue = atof(readLineFromInput);
		
		if(bRequirement < currentBframeValue) {
		  bframes = i;
		  bRequirement = bframethreshold;
		} else {
		  bRequirement += bframethreshold;
		  bRequirement -= currentBframeValue;
		}
		
		//progres to the next number
		findSequence("%", 1);
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
  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\nBframes Value Picked: %u", bframes);
  printf(nextPartOfLog);
  fputs(nextPartOfLog, b264log);  
  
  ///////////////////
  //START CRF TESTS//
  ///////////////////
  int crfIterations = 1;
  float db;
  float bitrate = 0.0;
  while(crfIterations <= 3) {
    //the x264 command; uses the same avs as the bframes testing
    snprintf(x264Command, sizeof(x264Command), "%s %s -o - | %s --preset placebo --no-mbtree --rc-lookahead 250 --subme 11 --deblock %i:%i --aq-mode %u --aq-strength %g --qcomp %g --me %s --merange %u --ref %u --bframes %u --crf %g --no-psy --ssim -o %s/crftest%u.mkv - --demuxer y4m NUL 2>&1", avs2yuvloc, roughAvsName, x264loc, deblock_alpha, deblock_beta, aqmode, aqs, qcomp, me, merange, ref, bframes, crf, logdir, crfIterations);
    
	//print to b264log the command and context information
    snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\n\nStarting Crf Test %u, previous bitrate %g, trying crf %g\n", crfIterations, bitrate, crf);
	printf(nextPartOfLog);
	snprintf(nextPartOfLog, sizeof(nextPartOfLog), "%sCrf Test Command: %s", nextPartOfLog, x264Command);
	fputs(nextPartOfLog, b264log);
	
	//create the x264 log so that one can start printing to it.
	snprintf(x264logName, sizeof(x264logName), "%s/crftest%u.log", logdir, crfIterations);
    x264Process = popen(x264Command, "r");
	x264log = fopen(x264logName, "w");
	
	//clears sequenceFound, which is probably 1. This is only so that the loop starts in the first place.
    //the first 2 while loops keep x264 from outputting 100s of current bitrate statements. Comment them out to see what I mean..
    sequenceFound = 0;
    while(sequenceFound == 0) {
	  fgets(readLineFromInput, sizeof(readLineFromInput), x264Process);
      fputs(readLineFromInput, x264log);
	  findSequence("profile", 7);
    } 
  
    sequenceFound = 0;
    while(sequenceFound == 0) {
      fgets(readLineFromInput, sizeof(readLineFromInput), x264Process);
	  findSequence("x264", 4);
    }
  
    fputs("\nx264", x264log);
    fputs(readLineFromInput, x264log);
    fputs("\n", x264log);
  
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
	
	//x264Process has terminated and all the output has been stored in the log
    fclose(x264log);
    pclose(x264Process); 
	
	//we are looking for a specific db relative to the bits per pixel (the number of bits in the whole video divided by the number of pixels in the whole video - which is the resolution * the number of frames)
	//a good bpp is usually between .15 and .35, though values outside of this are not uncommon b/c videos vary drastically in compressability
	//if the bpp is high and the db is high, that probably means the video is more compressible, and so bits can be removed. If the bppf is low and the db is low, that probably means that the video is less compressible and needs more bits.
	//the function is linear such that as a video has more bits, the target db for that video goes down.
	//the equation, which I would typically render in 1 line of code, has been changed to 5 lines of code so that it's more clear how everything works.
	float dbAtZeroBitrate = 20.5;
	float dbReductionPerIncreaseInBPP = 1.4;
	float adjustmentConstant = 1.5;
	float bpp = (bitrate/framerate)/(encodeWidth*encodeHeight)*8192;//without the 8192, we are actually measuring kilobits per pixel
	float desiredDBatCurrentBitrate = dbAtZeroBitrate - dbReductionPerIncreaseInBPP*bpp;
	float crfAdjustment = (db-desiredDBatCurrentBitrate)/adjustmentConstant; //adjust crf in the correct direction to move closer to desired db. the adjustmentConstant is arbitrary and poorly understood. 1.3 simply seems to work better than 1 (otherwise the crf tends to overshoot the goal)
	
	//print to the log information about how the crfAdjustment equation functioned.
	//this is to help refine the equation, because it's not really well understood and probably not optimal
	snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\ndbAtZeroBitrate: %g\ndbReductionPerIncreaseInBPP: %g\nadjustmentConstant: %g\nbppf: %g\ndb: %g\ndesiredDBatCurrentBitrate: %g\ncrfAdjustment: %g\nold crf: %g\nnew crf: %g\nbitrate: %g", dbAtZeroBitrate, dbReductionPerIncreaseInBPP, adjustmentConstant, bpp, db, desiredDBatCurrentBitrate, crfAdjustment, crf, crf+crfAdjustment, bitrate);
	fputs(nextPartOfLog, b264log);
	
	//if the variance is less than .4, we are done
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
  float newdb = 0;
  int qcompState = 0; //1 = moving in a + direction, -1 = moving in a - direction, 2 = have been moving in a positive direction, 0 means there is no previous encode to compare to
  bitrate = 0;
  db = 0;
  while(qcompIteration <= 4) {
    //right now we are just going to try and approximate the bitrate by moving the crf an appropriate amount. This will cause problems if the crf approximation is off. In the future, algorithms can be added/changed so that the bitrate is closer.
	snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\n\nStarting Qcomp Test %u, qcomp %g\n", qcompIteration, qcomp);
    fputs(nextPartOfLog, b264log);
    printf(nextPartOfLog);
	
    snprintf(x264Command, sizeof(x264Command), "%s %s -o - | %s --preset placebo --no-mbtree --rc-lookahead 250 --subme 11 --deblock %i:%i --aq-mode %u --aq-strength %g --qcomp %g --me %s --merange %u --ref %u --bframes %u --crf %g --psy-rd %g:%g --ssim -o %s/qcomptest%u.mkv - --demuxer y4m NUL 2>&1", avs2yuvloc, roughAvsName, x264loc, deblock_alpha, deblock_beta, aqmode, aqs, qcomp, me, merange, ref, bframes, crf, psyr, psyt, logdir, qcompIteration);
	
	//print to b264log the command and context information
    snprintf(nextPartOfLog, sizeof(nextPartOfLog), "Qcomp Test Command: %s", x264Command);
	fputs(nextPartOfLog, b264log);
	
	//create the x264 log so that one can start printing to it.
	snprintf(x264logName, sizeof(x264logName), "%s/qcomptest%u.log", logdir, qcompIteration);
    x264Process = popen(x264Command, "r");
	x264log = fopen(x264logName, "w");
	
	//clears sequenceFound, which is probably 1. This is only so that the loop starts in the first place.
    //the first 2 while loops keep x264 from outputting 100s of current bitrate statements. Comment them out to see what I mean..
    sequenceFound = 0;
    while(sequenceFound == 0) {
	  fgets(readLineFromInput, sizeof(readLineFromInput), x264Process);
      fputs(readLineFromInput, x264log);
	  findSequence("profile", 7);
    } 
  
    sequenceFound = 0;
    while(sequenceFound == 0) {
      fgets(readLineFromInput, sizeof(readLineFromInput), x264Process);
	  findSequence("x264", 4);
    }
  
    fputs("\nx264", x264log);
    fputs(readLineFromInput, x264log);
    fputs("\n", x264log);
  
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
	
	//x264Process has terminated and all the output has been stored in the log
    fclose(x264log);
    pclose(x264Process);
	
	snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\ndb: %g\nnewdb: %g", db, newdb);
	fputs(nextPartOfLog, b264log);

	//this part is a work in progress
	//no additional comments at this time
	float standardChangeInCrf = .125;
	float standardChangeInQcomp = .05;
	if(qcompState == 0) { //first iteration only
	  qcompState = 1;
	  qcomp += standardChangeInQcomp;
	  crf += standardChangeInCrf;
	  db = newdb;
	}else if(newdb > db && qcompState > 0) { //qcomp has been growing, and should keep growing as indicated by newdb vs. db
	  qcomp += standardChangeInQcomp;
	  crf += standardChangeInCrf;
	  qcompState = 2;
	  db = newdb;
	} else if(newdb > db && qcompState < 0) { //qcomp has been shrinking, and should keep shrinking as indicated by newdb vs. db
	  qcomp -= standardChangeInQcomp;
	  crf -= standardChangeInCrf;
	  qcompState = -2;
	  db = newdb;
	} else if(newdb < db && qcompState == 1) { //second iteration only, and called only if qcomp needs to shrink
	  qcomp -= standardChangeInQcomp*2;
	  crf -= standardChangeInCrf*2;
	  qcompState = -1;
	} else if(newdb < db && qcompState < 0) { //qcomp has been growing, but it's time to stop
	  qcomp += standardChangeInQcomp;
	  crf += standardChangeInCrf;
	  qcompIteration += 5;
	} else if(newdb < db && qcompState > 0) { //qcomp has been shrinking, but it's time to stop
	  qcomp -= standardChangeInQcomp;
	  crf -= standardChangeInCrf;
	  qcompIteration += 5;
	}
	
    qcompIteration++;
  }
  
  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\nFinal Qcomp Value: %g", qcomp);
  fputs(nextPartOfLog, b264log);
  printf(nextPartOfLog);
}

int main(int argci, char* argvi[])
{
  printf("\n==================================================\nProgram Start\n");
  //read config file first, because it has lowest precedence
  if(readConfigFiles(argci,argvi) != 1)
    return -1; 
  
  //create the directory to store all log info
  mkdir(logdir);
  
  //create the log to put comprehensive information about the running of the program
  char b264logName[600];
  snprintf(b264logName, sizeof(b264logName), "%s/b264log.log", logdir);
  b264log = fopen(b264logName, "w");

  //print out how the program interpreted the config file.
  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "Configuration as read from the config files (cli for --config and --x264cur included):\nConfigloc: %s\nx264loc: %s\navs2yuvloc: %s\nlogdir: %s\nx264def: %s\nx264cur: %s\navs: %s\nbft: %g\npsize: %u\nmode: %u\ndeblock: %i:%i\naq: %i:%g\npsy-rd: %g:%g\nqcomp: %g\nme: %s:%u\nref: %u\nbframes: %u\ncrf: %g", configloc, x264loc, avs2yuvloc, logdir, x264defaults, x264current, initialavs, bframethreshold, psize, modetype, deblock_alpha, deblock_beta, aqmode, aqs, psyr, psyt, qcomp, me, merange, ref, bframes, crf);
  fputs(nextPartOfLog, b264log);

  //read cli, because cli args have highest precedence
  if(readCliArgs(argci, argvi) != 1)
    return -1; 
	
  //print out final settings to confirm that everything works correctly	
  snprintf(nextPartOfLog, sizeof(nextPartOfLog), "\n\nConfiguration after reading cli:\nConfigloc: %s\nx264loc: %s\navs2yuvloc: %s\nlogdir: %s\nx264def: %s\nx264cur: %s\navs: %s\nbft: %g\npsize: %u\nmode: %u\ndeblock: %i:%i\naq: %i:%g\npsy-rd: %g:%g\nqcomp: %g\nme: %s:%u\nref: %u\nbframes: %u\ncrf: %g", configloc, x264loc, avs2yuvloc, logdir, x264defaults, x264current, initialavs, bframethreshold, psize, modetype, deblock_alpha, deblock_beta, aqmode, aqs, psyr, psyt, qcomp, me, merange, ref, bframes, crf);
  fputs(nextPartOfLog, b264log);
  
  //resolution, ref frames, etc. x264 is not called
  if(findEasyValues() != 1)
    return -1;
    
  //use x264 and the output to get a rough idea of the best settings
  //should run comparatively quickly (encoding a total of maybe 30% of the movie)
  if(findRoughValues() != 1)
    return -1;
  
  fclose(b264log);
  printf( "\n\nProgram End \n==================================================\n" );
}
