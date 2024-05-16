// importing required libraries
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <hw/pci.h>
#include <hw/inout.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h> 
#include <sys/resource.h>

// data registers, standard no change
#define	INTERRUPT	  iobase[1] + 0		// Badr1 + 0 : also ADC register
#define	MUXCHAN		  iobase[1] + 2		// Badr1 + 2
#define	TRIGGER		  iobase[1] + 4		// Badr1 + 4
#define	AUTOCAL		  iobase[1] + 6		// Badr1 + 6
#define 	DA_CTLREG	  iobase[1] + 8		// Badr1 + 8

#define	AD_DATA	    iobase[2] + 0		// Badr2 + 0
#define 	AD_FIFOCLR	iobase[2] + 2		// Badr2 + 2

#define	TIMER0		  		iobase[3] + 0		// Badr3 + 0
#define	TIMER1		  		iobase[3] + 1		// Badr3 + 1
#define	TIMER2		  		iobase[3] + 2		// Badr3 + 2
#define	COUNTCTL	  		iobase[3] + 3		// Badr3 + 3
#define	DIO_PORTA	  	iobase[3] + 4		// Badr3 + 4
#define	DIO_PORTB	  	iobase[3] + 5		// Badr3 + 5
#define	DIO_PORTC	  	iobase[3] + 6		// Badr3 + 6
#define	DIO_CTLREG		iobase[3] + 7		// Badr3 + 7
#define	PACER1		  		iobase[3] + 8		// Badr3 + 8
#define	PACER2		  		iobase[3] + 9		// Badr3 + 9
#define	PACER3		  		iobase[3] + a		// Badr3 + a
#define	PACERCTL	  		iobase[3] + b		// Badr3 + b

#define 	DA_Data		iobase[4] + 0		// Badr4 + 0
#define	DA_FIFOCLR	iobase[4] + 2		// Badr4 + 2


//+++++++++++++++++++++++++++++++++++++++++++++++++
// GLOBAL VARIABLES
//+++++++++++++++++++++++++++++++++++++++++++++++++

int badr[5];			//PCI 2.2 assigns 6 IO base addresses
void *hdl;

uintptr_t iobase[6];
uintptr_t dio_in;
uint16_t adc_in[2];

unsigned int i, count
;
unsigned short chan;

unsigned int data[100];
float delta, dummy, overflow, steps=20.0;
	// Define number of steps


// structure for waveform parameter
struct Parameters
{
	int wave;
	float freq;
		// range 0 to __
	float amp;
		// range 0 to __
	float mean;

	// range 0 to __
	int delay_val;
	float delay_scale;
} wave_para;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

//+++++++++++++++++++++++++++++++++++++++++++++++++
// FUNCTION PROTOTYPES
//+++++++++++++++++++++++++++++++++++++++++++++++++


float inputchecker(char input[]); // To Convert String to Float if the value is valid
void settings_display();
void signal_handler_cc();
void signal_handler_cv();
void PCI_setup()
;
void sine_wave();
void square_wave();
void sawtooth_wave();
void triangular_wave();
void* waveform_output ();
void *switches_input()
;
void save_file(char *filename, FILE *fp, char *data);
void save_enter_filename();
void read_file(char *filename, FILE *fp);
void read_enter_filename();


//+++++++++++++++++++++++++++++++++++++++++++++++++
// MAIN THREAD
//+++++++++++++++++++++++++++++++++++++++++++++++++

int main(int argc, char** argv)
{

//+++++++++++++++++++++++++++++++++++++++++++++++++
// CHECKS FOR MAIN ARGUMENTS
//+++++++++++++++++++++++++++++++++++++++++++++++++
	
	int ret, i=1, choice;
	int wave_check = 0, freq_check = 0, amp_check = 0, mean_check = 0, exit_check = 0; // To count number of times a certain type of argument is ran
	int wave_temp;
	float freq_temp, amp_temp, mean_temp;
	char **p_to_arg = &argv[1]; 				// Start looking from 2nd argument
	pthread_t thread1; 								// For settings_display 0 = Triangular, 1 = Sawtooth, 2 = Square, 3 = Sine
    char checker[100];  // To hold user inputs before checking


	/*
	LEGEND FOR WAVEFORM TYPE
	1 = Sine
	2 = Square
	3 = Sawtooth
	4 = Triangular
	*/

	// Check if too little arguments
	if (argc < 5)
	{
		printf("More arguments required\n");
		printf("\nVarious Waveforms are available: \n Sine wave: -sine\n Triangular wave: -tri\n Square wave: -square\n Sawtooth wave: -saw\n", argv[0]); 
		printf("\nUsage: %s -wave -freq -amp -mean\n", argv[0]);
		printf("Example: %s -sine -f1 -a2.68 -m1\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	else if (argc == 5)  // Expecting 5 arguements total
	{
		// Loop through each argument
		while(i < 5 && (*p_to_arg)[0] == '-') // Check if have '-' and run till 4 arguments
		{
			if (strcmp(argv[i], "-sine") == 0)
			{
				wave_temp = 1; // Take 1 as Sine Wave
				wave_check += 1;
			}
			else if (strcmp(argv[i], "-square") == 0)
			{
				wave_temp = 2; // Take 2 as Square Wave
				wave_check += 1;
			}
			else if (strcmp(argv[i], "-saw") == 0)
			{
				wave_temp = 3; // Take 3 as Sawtooth Wave
				wave_check += 1;
			}
			else if (strcmp(argv[i], "-tri") == 0)
			{
				wave_temp = 4; // Take 4 as Triangular Wave
				wave_check += 1;
			}
			else if ((*p_to_arg)[1] == 'f')
			{
				// Check if valid frequency term
				freq_temp = inputchecker(*p_to_arg);
				if(freq_temp < 0 || freq_temp > 50)
				{
					printf("Invalid Frequency, format: -fXX where XX is number in range 0 to 50\n");
					exit(EXIT_FAILURE);
				}
				else { freq_check += 1; }
			}
			else if ((*p_to_arg)[1] == 'a')
			{
				// Check if valid amplitude term
				amp_temp = inputchecker(*p_to_arg);
				if (amp_temp < 0 || amp_temp > 2.69)
				{
					printf("Invalid Amplitude, format: -aXX where XX is number in range 0 to 2.68\n");
					exit(EXIT_FAILURE);
				}
				else { amp_check += 1; }
			}
			else if ((*p_to_arg)[1] == 'm')
			{
				// Check if valid mean term
				mean_temp = inputchecker(*p_to_arg);
				if(mean_temp < 0)
				{
					printf("Invalid Mean, format: -mXX where XX is a positive number\n");
					exit(EXIT_FAILURE);
				}
				else { mean_check += 1; }
			}
			else
			{
				// Invalid argument
				printf("Invalid Argument: %s\n", argv[i]);
				exit(EXIT_FAILURE);
			}
			p_to_arg += 1; // Move to next argument
			i += 1; // Increase count
		}

		
		// Check if all checks report okay
		if (wave_check == 1 && freq_check == 1 && amp_check == 1 && mean_check == 1)
		{
			// All checks need to be one otherwise the user has entered some argument more than once
			// Values here can be used for the main program
			//printf("Wave: %d, Frequency: %f, Amplitude: %f, Mean: %f\n", wave, freq, amp, mean);
	// For debugging
			
			// Update global structure for waveform parameters
			wave_para.wave = wave_temp;
			wave_para.freq = freq_temp;
			wave_para.amp = (0xFFFF/2.68) * amp_temp;
	// Converts 0 to 2.68 range to 0x0000 to 0xFFFF
			wave_para.mean = mean_temp;
	
		wave_para.delay_val = 1000/(freq_temp*steps);
		// Convert frequency to delay in ms
			wave_para.delay_scale = 1.0;	// Set default to 1 first
			
			// Clear the screen
			system("clear");
		}
		else	// Entered argument more than once
		{
	
			if (wave_check < 1) { printf("No valid wave argument detected, valid waves: -tri, -saw, -square, -sine\n"); }
			if (wave_check > 1) {printf("Wave argument entered more than once\n"); }
			if (freq_check > 1) { printf("Frequency argument entered more than once\n"); }
			if (amp_check > 1) { printf("Amplitude argument entered more than once\n"); }
			if (mean_check > 1) { printf("Mean argument entered more than once\n"); }
			printf("Usage: %s -wave -freq -amp -mean\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	else
	// If too many arguments were entered
	{
		printf("Too many arguments\n");
		printf("Usage: %s -wave -freq -amp -mean\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	

//+++++++++++++++++++++++++++++++++++++++++++++++++
// JON'S SECTION
//+++++++++++++++++++++++++++++++++++++++++++++++++

	PCI_setup();
	
	// Setup signal responses
	signal(SIGINT, signal_handler_cc);
		// for Ctrl+C
	signal(SIGTSTP, signal_handler_cv);
	// for Ctrl+Z
	
	
	// Start output Thread to output waveform
	if (pthread_create(NULL, NULL, &waveform_output, NULL) !=0)
	{
		perror("Failed to create thread");
	}  
	
	// Start Thread to read switch inputs
	if (pthread_create(NULL, NULL, &switches_input, NULL) !=0)
	{
		perror("Failed to create thread");
	} 
	
	// Print current waveform settings
	settings_display();
		// Get user input
	while(1)
	{
		pause();	// Hold main thread here until Ctrl+Z
		
		// Print Main Menu on Terminal
		system("clear");
		printf("MAIN MENU\n");
		printf("Select a number (1-3)\n");
		printf("1. Choose type of Waveform\n");
		printf("2. Save data to file\n");
		printf("3. Read file and load\n");

		
		// Get user input
	    scanf("%s", &checker[2]);
	    while ((getchar()) != '\n');
        choice = inputchecker(checker);
	    
        // Repeat prompt if user input error
	    while (choice<1 || choice>3)
	    {
	    	printf("Invalid input, please try again.\nEnter values from 1 to 3.\n");
	    	
            // Get user input
            scanf("%s", &checker[2]);
            while ((getchar()) != '\n');
            choice = inputchecker(checker);
	    }

		
		if (choice==1)
		{
			printf("Choose type of Waveform: \n(1 for Sine, 2 for Square, 3 for Sawtooth, 4 for Triangular): \n");
		    
            // Get user input
            scanf("%s", &checker[2]);
            while ((getchar()) != '\n');
            wave_temp = inputchecker(checker);

            // Repeat prompt if user input error
            while (wave_temp<1 || wave_temp>4)
            {
                printf("Invalid input, please try again.\nEnter values from 1 to 4.\n");
                
                // Get user input
                scanf("%s", &checker[2]);
                while ((getchar()) != '\n');
                wave_temp = inputchecker(checker);
            }
		    
            pthread_mutex_lock(&mutex);
            wave_para.wave = wave_temp;
            pthread_mutex_unlock(&mutex);
		    
	  	}
		else if (choice==2)
		{
    		save_enter_filename();
	  	}		
		else if (choice==3)
		{
    		read_enter_filename();
		}
		else
		{
			printf("Error has occured\n");
		}
		
		settings_display();
	}
	
	
//**********************************************************************************************
// Reset DAC to default : 5v
//**********************************************************************************************
	
	out16(DA_CTLREG,(short)0x0a23);	
	out16(DA_FIFOCLR,(short) 0);			
	out16(DA_Data, 0x8fff);						// Mid range - Unipolar																											
	  
	//out16(DA_CTLREG,(short)0x0a43);	
	//out16(DA_FIFOCLR,(short) 0);			
	//out16(DA_Data, 0x8fff);				
																																							
	printf("\n\nExit Demo Program\n");
	pci_detach_device(hdl);

	return (0);
}
//---------------------END OF MAIN---------------------


//+++++++++++++++++++++++++++++++++++++++++++++++++
// FUNCTION: USER INPUT CHECKER
//+++++++++++++++++++++++++++++++++++++++++++++++++

float inputchecker(char *input)
{
	float result;
	int valid = 1, decimal_dotcount = 0, i;

	for(i = 2; input[i] != '\0'; i++) // Check all char in string, "\0" represents end of string
	{
		if (!isdigit(input[i]) && input[i] != '.') { valid = 0; } // Check input for special character
		if (input[i] == '.') { decimal_dotcount++; } // Count no. of dots
	}

	if ((valid) && (decimal_dotcount <= 1)) // Valid if only 1 dot and only numbers in input
	{
		// Convert the input string to a float
		sscanf(&(input)[2], "%f", &result);
		return result;
	}
	else { return -1; } // Exit function with invalid float in invalid range
}


//+++++++++++++++++++++++++++++++++++++++++++++++++
// Function: Settings display
//+++++++++++++++++++++++++++++++++++++++++++++++++

void settings_display()
{
	printf("\nTo Exit Program, press Ctrl + C or Flip 1st Left Switch.\n");
	printf("\nAdjust the Parameters using Potentiometer:\nMean: Flip 2nd Left Switch \nFrequency: Flip 3rd Left Switch \nAmplitude: Flip 4th Left Switch \n\nTo Enter Keyboard Menu, press Ctrl + Z \n");
	printf("+---------------------------------------+\n");
	printf("|           WAVEFORM SETTINGS           |\n");
	printf("+---------------------------------------+\n");
	switch(wave_para.wave)
	{
		case 1:
			printf("| Waveform: Sine\t\t\t|\n");
			break;
		case 2:
			printf("| Waveform: Square\t\t\t|\n");
			break;
		case 3:
			printf("| Waveform: Sawtooth\t\t\t|\n");
			break;
		case 4:
			printf("| Waveform: Triangular\t\t\t|\n");
			break;
	}
	
	printf("| Amplitude: 0x%x                   \t|\n", (int)wave_para.amp);
	printf("| Frequency(Hz): %-7.2f \t\t|\n", wave_para.freq);
	printf("| Mean: 0x%x                        \t|\n", (int)wave_para.mean);
	printf("+------------+------------+-------------+\033[A\n\n\n");
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Function: Signal to enter main menu based on Ctrl+Z
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void signal_handler_cv()
{
	
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Function: Signal to terminate based on Ctrl+C and T3 Switch
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void signal_handler_cc() 
{
	printf("Ctrl+C\n");
	exit(1);

/*	
  printf("\nHardware Termination Raised\n");

  terminate();

  // Cancel all threads except for the current one
  for (i = 0; i < numthreads; i++) {
      if (pthread_self() != thread[i]) {
          pthread_cancel(thread[i]);
          printf("Thread %ld is killed.\n", thread[i]);
      }
  }
  // Exit the current thread
  printf("Thread %ld is killed.\n", pthread_self());
  pthread_exit(NULL);
*/
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Function: Setup to interact with PCI hardware
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void PCI_setup()
{
  struct pci_dev_info info;

  // Initialise memory connect to PCI Attach and PCI_attach_device
  memset(&info,0,sizeof(info));
  if(pci_attach(0)<0){  // connect to PCI server
    perror("pci_attach");
    exit(EXIT_FAILURE);
  }

  info.VendorId=0x1307; // Vendor and Device ID
  info.DeviceId=0x01;

  if ((hdl=pci_attach_device(0, PCI_SHARE|PCI_INIT_ALL, 0, &info))==0) { // attach driver to PCI device
    perror("pci_attach_device");
    exit(EXIT_FAILURE);
  }

  // Determine Address of PCI Resource
  for(i=0;i<5;i++) {
    badr[i]=PCI_IO_ADDR(info.CpuBaseAddress[i]);
    //printf("Badr[%d] : %x\n", i, badr[i]);
  }

  for(i=0;i<5;i++) {			// expect CpuBaseAddress to be the same as iobase for PC
    iobase[i]=mmap_device_io(0x0f,badr[i]);
    //printf("Index %d : Address : %x ", i,badr[i]);
  	//printf("IOBASE  : %x \n",iobase[i]);
  }

  // Modify Thread Control Privity
  if(ThreadCtl(_NTO_TCTL_IO,0)==-1) {
    perror("Thread Control");
    exit(1);
  }

  // Initialise Board (ADC Port)
  out16(INTERRUPT,0x60c0);		// sets interrupts	 - Clears
  out16(TRIGGER,0x2081);			// sets trigger control: 10MHz, clear, Burst off, SW trig. default:20a0
  out16(AUTOCAL,0x007f);			// sets automatic calibration : default
  out16(AD_FIFOCLR,0); 			// clear ADC buffer
  out16(MUXCHAN,0x0D00);		// Write to MUX register-SW trigger,UP,DE,5v,ch 0-0
                            					// x x 0 0 | 1  0  0 1  | 0x 7   0 | Diff - 8 channels
                            					// SW trig |Diff-Uni 5v| scan 0-7| Single - 16 channels


 // Initialise Board (Digital Port)
  out8(DIO_CTLREG,0x90);		// Port A : Input Port B: Output
  dio_in=in8(DIO_PORTA); 					// Read Port A	
  //printf("Port A : %02x\n", dio_in);																												
  out8(DIO_PORTB, dio_in);					// output Port A value -> write to Port B 	
  
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Function: Generate Sine, Square, Sawtooth, Triangular Wave Array
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void sine_wave(){
  
  delta = (2.0*3.142)/steps;
  for(i=0; i<steps; i++)
  {
 
  if ((wave_para.amp + wave_para.mean)>65535) 
  {
  overflow = (wave_para.mean + wave_para.amp) - 65535;
  wave_para.mean = wave_para.mean - overflow;
  }
  dummy= ((sinf((float)(i*delta)) + 1.0) * wave_para.amp / 2)+wave_para.mean ;
  
  
  data[i]= (unsigned) dummy;								// add offset +  scale
  }
}

void square_wave(){
  for(i=0;i<steps;i++)
  {
  	if ((wave_para.amp + wave_para.mean)>65535) 
  	{
  		overflow = (wave_para.mean + wave_para.amp) - 65535;
  		wave_para.mean = wave_para.mean - overflow;
  	}
    if(i<=(0.5*steps)) dummy = (1* wave_para.amp)+wave_para.mean;
    else if(i>(steps*0.5)) dummy = (0* wave_para.amp)+wave_para.mean;
    data[i]= (unsigned) dummy;			
  }
}

void sawtooth_wave(){
  delta = 1.0/steps;
  for(i=0;i<steps;i++)
  {
  	if ((wave_para.amp + wave_para.mean)>65535) 
  	{
  		overflow = (wave_para.mean + wave_para.amp) - 65535;
  		wave_para.mean = wave_para.mean - overflow;
  	}
    dummy = ((i*delta)* wave_para.amp)+wave_para.mean; 	
    data[i]= (unsigned) dummy;			
  }
}

void triangular_wave(){
  delta = 2.0/steps;
  for(i=0;i<steps;i++)
  {
  	if ((wave_para.amp + wave_para.mean)>65535) 
  	{
  		overflow = (wave_para.mean + wave_para.amp) - 65535;
  		wave_para.mean = wave_para.mean - overflow;
  	}
    if(i<=(0.5*steps)) dummy = (delta * i * wave_para.amp)+wave_para.mean;
    if(i>(0.5*steps)) dummy = (wave_para.amp - ((i - (0.5*steps)) * wave_para.amp * delta))+wave_para.mean;
    data[i]= (unsigned) dummy;	
  }
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Thread: Output Waveform on Oscilloscope
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void* waveform_output () 
{
	//printf("Delay Value in thread: %d\n", *((int*)delay_value));
	// for debugging
	
  while(1)
  { 
  	//--------Check waveform type--------
  	if (wave_para.wave == 1){
    	sine_wave();
    }
	else if (wave_para.wave == 2){
		square_wave();
	}	
	else if (wave_para.wave == 3){
    	sawtooth_wave();
	}
	else if (wave_para.wave == 4){
    	triangular_wave();
	}
	else
	{
		printf("Invalid Input!");
		return;
	}
	

	//--------Output waveform data--------
  	for(i=0;i<steps;i++)
  	{
    	out16(DA_CTLREG,0x0a23);			// DA Enable, #0, #1, SW 5V unipolar		2/6
    	out16(DA_FIFOCLR, 0);					// Clear DA FIFO  buffer
    	out16(DA_Data,(short) data[i]);
    	
    	pthread_cond_signal(&cond);

    	delay(wave_para.delay_val * wave_para.delay_scale);	
  	}
  	
  }
  
  return (0);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Thread: Toggle Switches Input
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void *switches_input()
{

  unsigned int count;
  struct timespec delay_time = {0,100000000}; //100ms delay
  
  //------Save starting values---------
  float amp = wave_para.amp;
  float freq = wave_para.freq;
  float mean = wave_para.mean;
  
  
  while(1)  {
    dio_in=in8(DIO_PORTA); 					// Read Port A, Port A is Toggle Switches
	
	//-----------Debugging prints-----------
	//printf("dio_in: %x\n", dio_in);
	//printf("%d\n", 0x03 == 0x01);
	//printf("adc_in value: %x\n", adc_in[0]);
	//printf("Delay Scale value: %f\n", wave_para.delay_scale);
	
		
    //out8(DIO_PORTB, dio_in);					// output Port A value -> write to Port B, Port B is LED?

	
	//---------------Read Potentiometer---------------
	count=0x00;
    while(count<0x02){
		chan= ((count & 0x0f)<<4) | (0x0f & count);
        out16(MUXCHAN,0x0D00|chan);		// Set channel	 - burst mode off.
        nanosleep(&delay_time, NULL);											// allow mux to settle
        out16(AD_DATA,0); 							// start ADC 
        while(!(in16(MUXCHAN) & 0x4000));	
        adc_in[(int)count]=in16(AD_DATA);  
        count++; 
        nanosleep(&delay_time, NULL);
	}


	//---------------Toggle switch checks---------------
	
	pthread_mutex_lock(&mutex);
	pthread_cond_wait(&cond, & mutex);
	
	// 0 0 0 1 , only T0 switch ON, rest off
    if( dio_in == 0xf1) 					
    {
      wave_para.amp = adc_in[0];
      pthread_mutex_unlock(&mutex);
      
      system("clear");
      settings_display();
    }

	// 0 0 1 0, only T1 switch ON, rest off
    
    if( dio_in == 0xf2) 					
    {
      wave_para.delay_scale = adc_in[0] * 2.0 / 0xffff; //scale from 16 bits to 0 ~ 2
      wave_para.freq = freq * (1/wave_para.delay_scale);
      pthread_mutex_unlock(&mutex);
      
      system("clear");
      settings_display();
    }

	// 0 1 0 0, only T2 switch ON, rest off
    if( dio_in == 0xf4) 					
    {
      wave_para.mean = adc_in[0]; //scale from 16 bits to 0.00 ~ 1.00
      pthread_mutex_unlock(&mutex);
      
      system("clear");
      settings_display();
    }

	// 1 0 0 0, only T3 switch ON, rest off
    if( dio_in == 0xf8) 
    {
      raise(SIGINT); // T3 switch terminate program
    }
    

  //nanosleep(&delay_time, NULL);
  pthread_mutex_unlock(&mutex);
} 
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Function: Save Data to File 
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void save_file(char *filename, FILE *fp, char *data){
    strcat(filename, ".txt");

    if ((fp = fopen(filename, "w")) == NULL){
      perror("Cannot Open!\n\n");
      return;
    }

    if (fputs(data, fp) == EOF){
      perror("Cannot Write!\n\n");
      return;
    }

    fclose(fp);
    printf("File Saved!\n");
	}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Function: Enter Filename (Save)
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void save_enter_filename() {
    char filename[100]; // 100 character long filename
    char data[200];
	  FILE *fp;
    int holder;

    printf("Enter a filename: \n");
    holder = scanf("%99s", filename);

    if (holder != 1) {   // Invalid Input
        printf("Invalid input. Please enter a valid filename.\n");
        return;
    }

    sprintf(data,         // save data into string
            "\t\tAmp.\tMean\tFreq.\tWave\tDelayValue\tDelayScale\n"
            "Ch Parameter:\t%2.2f\t%2.2f\t%2.2f\t%d\t%d\t%2.2f\n\n",
            wave_para.amp, wave_para.mean, wave_para.freq, wave_para.wave, wave_para.delay_val, wave_para.delay_scale);

    save_file(filename,fp,data);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Function: Read Values from File
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void read_file(char *filename, FILE *fp) {
    int count;

    strcat(filename, ".txt");
    printf("\nFile Reading In Progress..\n");

    if ((fp = fopen(filename, "r")) == NULL) {
        perror("Cannot Open File!");
        return;
    }
    
    pthread_mutex_lock(&mutex);

    count = fscanf(fp,
          "%*s\t%*s\t%*s\t%*s\t%*s\t%*s\n" // Skip the header line
          "Ch Parameter:\t%f\t%f\t%f\t%d\t%d\t%f",
           &wave_para.amp, &wave_para.mean, &wave_para.freq, &wave_para.wave, &wave_para.delay_val, &wave_para.delay_scale);

    pthread_mutex_unlock(&mutex);
    
    if (count == 6) {
      printf("Values read from file:\nAmp: %2.2f Mean: %2.2f Freq %2.2f Wave: %d Delay Value: %d Delay Scale %2.2f\n", wave_para.amp, wave_para.mean, wave_para.freq, wave_para.wave, wave_para.delay_val, wave_para.delay_scale);
    }
    else {
      printf("Error Reading values from file\n");
    }

    fclose(fp);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Function: Enter Filename (Read)
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void read_enter_filename() {
    char filename[100];
    FILE *fp;

    printf("Enter the filename: \n");
    scanf("%s", filename);

    //return main menu if input = 0
    if (strcmp(filename, "0") == 0)
      return;

    read_file(filename, fp);
}