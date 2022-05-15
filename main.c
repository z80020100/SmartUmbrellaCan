/*
 * Pin layout should be as follows:
 * Signal     Pin              Pin
 *            Arduino Uno      MFRC522 board      Color
 * ----------------------------------------------------
 * Reset      9                RST				  White
 * SPI SS     10               SDA				  Red
 * SPI MOSI   11               MOSI				  Orange
 * SPI MISO   12               MISO				  Yellow
 * SPI SCK    13               SCK				  Blue
 */

#include "Arduino.h"
#include <SPI.h> // the sensor communicates using SPI
#include <stdlib.h>
#include <string.h>

#define	uchar unsigned char // 8 bits
#define	uint  unsigned int // 16 bits

#define MAX_LEN 16 // for S50: 1 KB, organized in 16 sectors with 4 blocks of 16 bytes each (one block consists of 16 byte)

// The MFRC522 command word
#define PCD_IDLE              0x00               // no action; cancel the current command
#define PCD_AUTHENT           0x0E               // authentication key
#define PCD_RECEIVE           0x08               // receive data
#define PCD_TRANSMIT          0x04               // send data
#define PCD_TRANSCEIVE        0x0C               // send and receive data
#define PCD_RESETPHASE        0x0F               // reset
#define PCD_CALCCRC           0x03               // CRC calculation

// Mifare_One card command word
#define PICC_REQIDL           0x26               // look for antenna region does not enter hibernation
#define PICC_REQALL           0x52               // look for the antenna all the cards in the region
#define PICC_ANTICOLL         0x93               // anti-collision
#define PICC_SElECTTAG        0x93               // election card
#define PICC_AUTHENT1A        0x60               // verify A key
#define PICC_AUTHENT1B        0x61               // verify B key
#define PICC_READ             0x30               // read block
#define PICC_WRITE            0xA0               // write block
#define PICC_DECREMENT        0xC0               
#define PICC_INCREMENT        0xC1               
#define PICC_RESTORE          0xC2               // adjust the block data to buffer
#define PICC_TRANSFER         0xB0               // save the buffer data
#define PICC_HALT             0x50               // sleep

// MFRC522 communication error code returned
#define MI_OK                 0
#define MI_NOTAGERR           1 // No tag error
#define MI_ERR                2

//------------MFRC522 Register------------
// Page 0: Command and Status
#define     Reserved00            0x00    
#define     CommandReg            0x01    
#define     CommIEnReg            0x02    
#define     DivlEnReg             0x03    
#define     CommIrqReg            0x04    
#define     DivIrqReg             0x05
#define     ErrorReg              0x06    
#define     Status1Reg            0x07    
#define     Status2Reg            0x08    
#define     FIFODataReg           0x09
#define     FIFOLevelReg          0x0A
#define     WaterLevelReg         0x0B
#define     ControlReg            0x0C
#define     BitFramingReg         0x0D
#define     CollReg               0x0E
#define     Reserved01            0x0F
// Page 1: Command
#define     Reserved10            0x10
#define     ModeReg               0x11
#define     TxModeReg             0x12
#define     RxModeReg             0x13
#define     TxControlReg          0x14
#define     TxAutoReg             0x15
#define     TxSelReg              0x16
#define     RxSelReg              0x17
#define     RxThresholdReg        0x18
#define     DemodReg              0x19
#define     Reserved11            0x1A
#define     Reserved12            0x1B
#define     MifareReg             0x1C
#define     Reserved13            0x1D
#define     Reserved14            0x1E
#define     SerialSpeedReg        0x1F
// Page 2: CFG
#define     Reserved20            0x20  
#define     CRCResultRegM         0x21
#define     CRCResultRegL         0x22
#define     Reserved21            0x23
#define     ModWidthReg           0x24
#define     Reserved22            0x25
#define     RFCfgReg              0x26
#define     GsNReg                0x27
#define     CWGsPReg	          0x28
#define     ModGsPReg             0x29
#define     TModeReg              0x2A
#define     TPrescalerReg         0x2B
#define     TReloadRegH           0x2C
#define     TReloadRegL           0x2D
#define     TCounterValueRegH     0x2E
#define     TCounterValueRegL     0x2F
// Page 3: TestRegister
#define     Reserved30            0x30
#define     TestSel1Reg           0x31
#define     TestSel2Reg           0x32
#define     TestPinEnReg          0x33
#define     TestPinValueReg       0x34
#define     TestBusReg            0x35
#define     AutoTestReg           0x36
#define     VersionReg            0x37
#define     AnalogTestReg         0x38
#define     TestDAC1Reg           0x39  
#define     TestDAC2Reg           0x3A   
#define     TestADCReg            0x3B   
#define     Reserved31            0x3C   
#define     Reserved32            0x3D   
#define     Reserved33            0x3E   
#define     Reserved34			  0x3F
//-----------------------------------------------

const int chipSelectPin = 10; // SPI_SS
const int NRSTPD = 9; // RESET

int ENA = 1, IN1 = 2, IN2 = 3; // Set Arduino pins for L298
int total_time = 0; // Record rotation time

int green = 4; // Green LED

int ubl_1 = 5, ubl_2 = 6; // umbrella digital read pin

// 4-byte card serial number, 5th byte is checksum byte
uchar serNum[5] = {0};
uchar writeDate[16] = "umbrella";
// Password(Key A) of each sector, the total number of sectors is 16, the password of each sector is 6 bytes
uchar sectorKeyA[16][6] =
{
								{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
								{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
								{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
}; // Default Key A: {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

/*
 * for sectorNewKey[i], i = 3, 7, 11, ... , 63
 * 1st byte to 6th byte is Key A
 * 7th byte to 10th byte is Access Bits
 * 11th byte to 16 bytes is Key B
 */
uchar sectorNewKey[16][16] =
{
								{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
                                {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xff,0x07,0x80,0x69, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
                                {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xff,0x07,0x80,0x69, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
};					

/* MFRC522 defined function */
void MFRC522_Halt(void);
uchar MFRC522_Write(uchar blockAddr, uchar *writeData);
uchar MFRC522_Read(uchar blockAddr, uchar *recvData);
uchar MFRC522_Auth(uchar authMode, uchar BlockAddr, uchar *Sectorkey, uchar *serNum);
uchar MFRC522_SelectTag(uchar *serNum);
void CalulateCRC(uchar *pIndata, uchar len, uchar *pOutData);
uchar MFRC522_Anticoll(uchar *serNum);
uchar MFRC522_ToCard(uchar command, uchar *sendData, uchar sendLen, uchar *backData, uint *backLen);
uchar MFRC522_Request(uchar reqMode, uchar *TagType);
void MFRC522_Init(void);
void MFRC522_Reset(void);
void AntennaOff(void);
void AntennaOn(void);
void ClearBitMask(uchar reg, uchar mask);
void SetBitMask(uchar reg, uchar mask);
uchar Read_MFRC522(uchar addr);
void Write_MFRC522(uchar addr, uchar val);
void reg_read_write_test(uchar addr, uchar val);
void card_type_indentify(uint cardTypeID);

/* L298 defined function */
void L298_init();
void forward(double time);
void reversal(double time);
void slow_stop();
void reset_motor();

/* Database defined function */
void insert(char *colum1, int value1, char *colum2, int value2, char *colum3, int value3, char *ip, char *port, char *table);
void retrieval_user_status(char *ip, char *port, char *SN);
						   
void setup()
{
	SPI.begin();  // start the SPI library
	pinMode(chipSelectPin, OUTPUT); // Set digital pin 10 as OUTPUT to connect it to the RFID ENABLE pin(SDA or SS or CS)
    digitalWrite(chipSelectPin, LOW); // Activate the RFID reader
	pinMode(NRSTPD, OUTPUT); // Set digital pin 5, Not Reset and Power-down
    digitalWrite(NRSTPD, HIGH);
	
	MFRC522_Reset();
	
	// MFRC522 Register W/R Test
	reg_read_write_test(TPrescalerReg, 0x3E);
	
	puts("MFRC522 Initialization...");
	MFRC522_Init();
	
	puts("L298 Initialization...");
	L298_init();
	
	pinMode(green, OUTPUT);
	
	pinMode(ubl_1, INPUT);
	pinMode(ubl_2, INPUT);
}

void loop()
{
	uchar i;
	uchar status;
    uchar str[MAX_LEN]; // temporary
    uchar cardSize; // record the card capacity
    uchar blockAddr; // select the operating block address: 0 to 63
	uint cardTypeID;
	int serialNumber; // RFID card serial number(integer)
	char SN[32]; // RFID card serial number(string)
	memset(SN, 0, sizeof(SN));
	memset(str, 0, sizeof(str));
	
	int userStatus = -1; // user borrrow/return status
	int ubl_1_v, ubl_2_v; // umbrella check value
	int umbrella; // initial the number of umbrella in can
		
	puts("Wait for a card...");
	// Looking for the card and return the card type to array str
	status = MFRC522_Request(PICC_REQIDL, str);
	if (status == MI_OK)
	{	puts("Find out a card...");
		cardTypeID = (str[0] << 8) + str[1];
		//printf("cardTypeID = 0x%X\n", cardTypeID);
		card_type_indentify(cardTypeID);
	}	
	else if(status == MI_NOTAGERR)
		puts("No tag error!");
	//if(status == MI_ERR)
		//puts("No Card.");
	
	// Anti-collision, return the 4-bytes card serial number , the 5th byte is check byte
	status = MFRC522_Anticoll(str);
	memcpy(serNum, str, 5);
	if (status == MI_OK)
	{
		FILE *fp = fopen("userStatus.txt", "w+");
		
		printf("The card's serial number (Hexadecimal separately): 0x%X 0x%X 0x%X 0x%X\n", serNum[0], serNum[1], serNum[2], serNum[3]);
		serialNumber = (serNum[0] << 24) + (serNum[1] << 16) + (serNum[2] << 8) + serNum[3];
		printf("The card's serial number (Decimal): %d\n", serialNumber);
		sprintf(SN, "%d", serialNumber);
		printf("SN: %s\n", SN); // int to string
		retrieval_user_status("140.112.42.93", "3000", SN);  // 向Database詢問使用者是否可借用, command: curl http://140.112.42.93:3000/users/serialNumber/status, //if return 0, user can borrow
		puts("");
		delay(1000);
		fscanf(fp, "%d", &userStatus);
		printf("userStatus = %d\n", userStatus);
		fclose(fp);
	}
	
	//userStatus = 0; // for test
	
	// Election card, return the card capacity
	cardSize = MFRC522_SelectTag(serNum);
	if(cardSize != 0)
		{ printf("Card size is %uK bits\n", cardSize); }
	
	// Read data for run the RFID read process complete.
	blockAddr = 7;	
	status = MFRC522_Auth(PICC_AUTHENT1A, blockAddr, sectorNewKey[blockAddr/4], serNum); // authentication
	if(status == MI_OK)
	{
		puts("Authentication successfully!");
		blockAddr = blockAddr - 3;
		//printf("Read data from <block %u>\n", blockAddr);
		status = MFRC522_Read(blockAddr, str);
		if(status == MI_OK)
		{
			//puts("The data is : ");
			for(i = 0; i < 16; i++)
			{
				//printf("%c", str[i]);
			}
			printf("Card data read complete.\n");
		}
	}
	MFRC522_Halt(); // command card into hibernation
	
	ubl_1_v = digitalRead(ubl_1); // check umbrella state
	ubl_2_v = digitalRead(ubl_2);
	umbrella = ubl_1_v + ubl_2_v;
	printf("The initial number of umbrella: %d\n", umbrella);
	
	if(userStatus == 0) // user can borrow umbrella
	{		

		//ubl_1_v = HIGH; // for test
		//ubl_2_v = HIGH; // for test
		printf("ubl_1_v = %d, ubl_2_v = %d\n", ubl_1_v, ubl_2_v);
		if(ubl_1_v == LOW && ubl_2_v == LOW)
		{
			puts("EMPTY!");
		}		
		else if(ubl_1_v == HIGH)
		{
			digitalWrite(green, HIGH);
			puts("START UNLOCK");
			forward(24); // unlock
			delay(10000);
			reversal(24); // lock
			puts("LOCKED");
			ubl_1_v = digitalRead(ubl_1);
			if(ubl_1_v == LOW)
			{
				insert("userCard", serialNumber, "stationId", 12, "action", 0, "140.112.42.93", "3000", "records"); // action = 0: borrow umbrella
				umbrella = umbrella - 1;
				printf("\nThe number of umbrella: %d\n", umbrella);
			}
		}
		else if(ubl_2_v == HIGH)
		{
			digitalWrite(green, HIGH);
			//forward(); // unlock
			delay(10000);
			//reversal(); // lock
			ubl_2_v = digitalRead(ubl_2);
			if(ubl_2_v == LOW)
			{	
				insert("userCard", serialNumber, "stationId", 12, "action", 0, "140.112.42.93", "3000", "records"); // action = 0: borrow umbrella
				umbrella = umbrella - 1;
				printf("\nThe number of umbrella: %d\n", umbrella);
			}
		}
		digitalWrite(green, LOW);
	}
	else if(userStatus == 1) // user can return umbrella
	{
		ubl_1_v = digitalRead(ubl_1); // check umbrella state
		ubl_2_v = digitalRead(ubl_2);
		if(ubl_1_v == HIGH && ubl_2_v == HIGH)
		{
			puts("FULL!");
		}				
		else if(ubl_1_v == LOW)
		{
			digitalWrite(green, HIGH);
			forward(24); // unlock
			delay(10000);
			reversal(24); // lock
			ubl_1_v = digitalRead(ubl_1);
			if(ubl_1_v == HIGH)
			{
				insert("userCard", serialNumber, "stationId", 12, "action", 1, "140.112.42.93", "3000", "records"); // action = 1: return umbrella
				umbrella = umbrella + 1;
				printf("\nThe number of umbrella: %d\n", umbrella);
			}
		}
		else if(ubl_2_v == LOW)
		{
			digitalWrite(green, HIGH);
			//forward(); // unlock
			delay(10000);
			//reversal(); // lock
			ubl_2_v = digitalRead(ubl_2);
			if(ubl_2_v == HIGH)
			{
				insert("userCard", serialNumber, "stationId", 12, "action", 1, "140.112.42.93", "3000", "records"); // action = 1: return umbrella
				umbrella = umbrella + 1;
				printf("\nThe number of umbrella: %d\n", umbrella);
			}
		}
		digitalWrite(green, LOW);
	}
	else if(userStatus == -1)
	{ puts("No user status information."); }
}

/* ----------MFRC522 function---------- */
/*
 * Function: Write_MFRC5200
 * Description: Write one byte data to a register of MFRC522
 * Input parameters: 
 *					addr - register address
 *					val  - value to be written
 */
void Write_MFRC522(uchar addr, uchar val)
{
	digitalWrite(chipSelectPin, LOW);

	// address format: 0XXXXXX0
	SPI.transfer((addr<<1) & 0x7E);
	SPI.transfer(val);
	
	digitalWrite(chipSelectPin, HIGH);
}

/*
 * Function: Read_MFRC5200
 * Description: Read one byte data from a register of MFRC522
 * Input parameters: 
 *					addr - register address
 * Return value: Returns the read data
 */
uchar Read_MFRC522(uchar addr)
{
	uchar val;

	digitalWrite(chipSelectPin, LOW);

	// address format: 1XXXXXX0
	SPI.transfer(((addr<<1)&0x7E) | 0x80);
	val = SPI.transfer(0x00);
	
	digitalWrite(chipSelectPin, HIGH);
	
	return val;
}

void SetBitMask(uchar reg, uchar mask)  
{
    uchar tmp;
    tmp = Read_MFRC522(reg);
    Write_MFRC522(reg, tmp | mask); // set bit mask
}

void ClearBitMask(uchar reg, uchar mask)  
{
    uchar tmp;
    tmp = Read_MFRC522(reg);
    Write_MFRC522(reg, tmp & (~mask)); //clear bit mask
} 

/* Turn on the antenna, the operation interval is at least 1ms */
void AntennaOn(void)
{
	uchar temp, result;

	temp = Read_MFRC522(TxControlReg);

	if(!(temp & 0x03))
	{
		result = temp | 0x03; 
		SetBitMask(TxControlReg, 0x03);
		temp = Read_MFRC522(TxControlReg);
		//printf("After SetBitMask, TxControlReg = %X\n", temp);
		if(temp == result)
			puts("Turn on the antenna successfully!");
		else
			puts("Turn on the antenna failed...");
	}
	else
		puts("The antenna already is on.");
}

/* Turn off the antenna, the operation interval is at least 1ms */
void AntennaOff(void)
{
	ClearBitMask(TxControlReg, 0x03);
}

void MFRC522_Reset(void)
{
    Write_MFRC522(CommandReg, PCD_RESETPHASE);
}

void MFRC522_Init(void)
{
	digitalWrite(NRSTPD,HIGH);
	MFRC522_Reset();	
	// Timer: TPrescaler * TreloadVal/6.78MHz = 24ms
    Write_MFRC522(TModeReg, 0x8D); // Tauto = 1; f(Timer) = 6.78MHz/TPreScaler
    Write_MFRC522(TPrescalerReg, 0x3E); // TModeReg[3..0] + TPrescalerReg
    Write_MFRC522(TReloadRegL, 30);
    Write_MFRC522(TReloadRegH, 0);
	Write_MFRC522(TxAutoReg, 0x40);	// 100%ASK
	Write_MFRC522(ModeReg, 0x3D); // CRC初始值0x6363
	//ClearBitMask(Status2Reg, 0x08); // MFCrypto1On = 0
	//MFRC522_HAL_write(RxSelReg, 0x86); // RxWait = RxSelReg[5..0]
	//MFRC522_HAL_write(RFCfgReg, 0x7F); // Configures the receiver gain, 48db
	AntennaOn(); // Turn on the antenna
}

/*
 * Function: MFRC522_Request
 * Description: look for the card and read the card type
 * Input parameters: 
 *					reqMode - to find the card the way,
 * 					TagType - return card type,
 * 					0x4400 = Mifare_UltraLight,
 * 					0x0400 = Mifare_One (S50),
 * 					0x0200 = Mifare_One (S70),
 * 					0x0800 = Mifare_Pro (X),
 * 					0x4403 = Mifare_DESFire
 * Return value:
 *					successful return MI_OK
 */ 
uchar MFRC522_Request(uchar reqMode, uchar *TagType)
{
	uchar status;  
	uint backBits; // the received data bits
	
	Write_MFRC522(BitFramingReg, 0x07); // TxLastBists = BitFramingReg[2..0]
	status = MFRC522_ToCard(PCD_TRANSCEIVE, &reqMode, 1, TagType, &backBits);
	if((status != MI_OK) || (backBits != 0x10))
	{    
		status = MI_ERR;
	}   
	return status;
}
 
/*
 * Function: MFRC522_ToCard
 * Description: RC522 and ISO14443 card communication
 * Input Parameters:
 *					command   - MFRC522 command word
 * 					*sendData - the data which will be sent to the card
 *					sendLen   - the length of sent data
 * 					*backData - the data which is received from the card
 * 					backLen   - the length of the received data
 * Return value: 
 *					successful return MI_OK
 */
uchar MFRC522_ToCard(uchar command, uchar *sendData, uchar sendLen, uchar *backData, uint *backLen)
{
    uchar status = MI_ERR;
    uchar irqEn = 0x00;
    uchar waitIRq = 0x00;
    uchar lastBits;
    uchar n;
    uint i;

    switch(command)
    {
		case PCD_AUTHENT: // certification card secret
		{
			irqEn = 0x12;
			waitIRq = 0x10;
			break;
		}
		case PCD_TRANSCEIVE: // send the data which is in FIFODataReg
		{
			irqEn = 0x77;
			waitIRq = 0x30;
			break;
		}
		default:
			break;
    }
   
    Write_MFRC522(CommIEnReg, irqEn|0x80); // allow the interrupt request
    ClearBitMask(CommIrqReg, 0x80); // Clear all interrupt request bit
    SetBitMask(FIFOLevelReg, 0x80);	// FlushBuffer = 1, the FIFO initialization
    Write_MFRC522(CommandReg, PCD_IDLE); // no action, cancels current command execution

	// write the sendData to the FIFODataReg
    for(i = 0; i < sendLen; i++)
    {   
		Write_MFRC522(FIFODataReg, sendData[i]);    
	}

	// Execute Command
	Write_MFRC522(CommandReg, command);
    if (command == PCD_TRANSCEIVE)
    {    
		SetBitMask(BitFramingReg, 0x80); // StartSend = 1, transmission of data starts
	}   
    
	//	wait for data transmission complete
	i = 2000;	// according to the clock frequency to adjust i, the maximum operation wait time for M1 card: 25ms
    do 
    {
		// CommIrqReg[7..0]
		// Set1 TxIRq RxIRq IdleIRq HiAlerIRq LoAlertIRq ErrIRq TimerIRq
        n = Read_MFRC522(CommIrqReg);
        i--;
    }
    while((i != 0) && !(n&0x01) && !(n&waitIRq));

    ClearBitMask(BitFramingReg, 0x80); // StartSend = 0
	
    if (i != 0)
    {    
        if(!(Read_MFRC522(ErrorReg) & 0x1B)) // BufferOvfl Collerr CRCErr ProtecolErr, [4 3 2 0]
        {
            status = MI_OK;
            if(n & irqEn & 0x01)
            {   
				status = MI_NOTAGERR; // no tag error
			}
            
			if(command == PCD_TRANSCEIVE)
            {
				n = Read_MFRC522(FIFOLevelReg); //	indicates the number of bytes stored in the FIFO
				//printf("%u bytes store in FIFO register\n", n);
				
				lastBits = Read_MFRC522(ControlReg) & 0x07; // read the [2 1 0] bit of the ControlReg, indicates the number of valid bits in the last received byte if this value is 000, the whole byte is valid
				//printf("%u bytes is valid in last received data, if the value is 0, the whole byte is valid\n", lastBits);
                if (lastBits)
                {   
					*backLen = (n-1)*8 + lastBits; // for the last byte, only has (lastBits bytes) data is valid, so (n-1)
				}
                else // lastBits = 0, the whole byte is valid
                {   
					*backLen = n*8;   
				}

                if(n == 0)
                {   
					n = 1;    
				}
                if(n > MAX_LEN)
                {
					n = MAX_LEN;   
				}
				
				// Read the received data in the FIFO
                for(i = 0; i < n; i++)
                {
					backData[i] = Read_MFRC522(FIFODataReg);
					//printf("backData[%u] = %u\n", i, backData[i]);
				}
            }
        }
        else
        {   
			status = MI_ERR;
		}
        
    }	
    //SetBitMask(ControlReg,0x80); // timer stops
    //Write_MFRC522(CommandReg, PCD_IDLE); 
	return status;
}

/*
 * Function: MFRC522_Anticoll
 * Description: anti-collision detection, and read the card serial number of the selected card
 * Input parameters: serNum - return the 4-byte card serial number, the 5th byte is the checksum byte
 * Return values: successful return MI_OK
 */
uchar MFRC522_Anticoll(uchar *serNum)
{
    uchar status;
    uchar i;
	uchar serNumCheck = 0;
    uint unLen;
    
    //ClearBitMask(Status2Reg, 0x08); // TempSensclear
    //ClearBitMask(CollReg,0x80);// ValuesAfterColl
	Write_MFRC522(BitFramingReg, 0x00); // TxLastBists = BitFramingReg[2..0]
 
    serNum[0] = PICC_ANTICOLL;
    serNum[1] = 0x20;
    status = MFRC522_ToCard(PCD_TRANSCEIVE, serNum, 2, serNum, &unLen);

    if(status == MI_OK)
	{
		// Check card serial number
		for(i = 0; i < 4; i++)
		{
			serNumCheck ^= serNum[i];
		}
		if(serNumCheck != serNum[i])
		{
			status = MI_ERR;
		}
    }
    //SetBitMask(CollReg, 0x80); // ValuesAfterColl = 1

	return status;
} 

/*
 * Function: CalulateCRC
 * Function Description: MF522 calculate the CRC
 * Input parameters: 
 *					pIndata  - to be reading a CRC data,
 *					len      - the length of the data,
 *					pOutData - calculated CRC results
 */
void CalulateCRC(uchar *pIndata, uchar len, uchar *pOutData)
{
    uchar i, n;

    ClearBitMask(DivIrqReg, 0x04); // CRCIrq = 0
    SetBitMask(FIFOLevelReg, 0x80); // clear FIFO pointer
    //Write_MFRC522(CommandReg, PCD_IDLE);

	// Write data to the FIFO
    for(i=0; i<len; i++)
    {
		Write_MFRC522(FIFODataReg, *(pIndata+i));   
	}
    Write_MFRC522(CommandReg, PCD_CALCCRC);

	// Wait for the CRC calculation is done
    i = 0xFF;
    do 
    {
        n = Read_MFRC522(DivIrqReg);
        i--;
    }
    while ((i!=0) && !(n&0x04)); // CRCIrq = 1

	// Read the CRC calculation results
    pOutData[0] = Read_MFRC522(CRCResultRegL);
    pOutData[1] = Read_MFRC522(CRCResultRegM);
}

/*
 * Function: MFRC522_SelectTag
 * Description: election card, and read the card memory capacity
 * Input parameters:
 *					serNum - incoming card serial number
 * Return values:
 *					successful return to card capacity in K bits
 */
uchar MFRC522_SelectTag(uchar *serNum)
{
    uchar i;
	uchar status;
	uchar size;
    uint recvBits;
    uchar buffer[9];

	//ClearBitMask(Status2Reg, 0x08); // MFCrypto1On = 0

    buffer[0] = PICC_SElECTTAG;
    buffer[1] = 0x70;
    for(i = 0; i < 5; i++)
    {
    	buffer[i+2] = *(serNum+i);
    }
	CalulateCRC(buffer, 7, &buffer[7]); // Remark: The CRC is split into two 8-bit registers, so the calculated result is stored in buffer[7] and buffer[8]
    status = MFRC522_ToCard(PCD_TRANSCEIVE, buffer, 9, buffer, &recvBits);
    if((status == MI_OK) && (recvBits == 0x18))
    {   
		size = buffer[0]; 
	}
    else
    {   
		size = 0;    
	}

    return size;
}

/*
 * Function: MFRC522_Auth
 * Description: Verify that the card password
 * Input parameters:
					authMode - Password Authentication Mode
						0x60 = verify the A key
						0x61 = verify the B key
					BlockAddr - block address
					Sectorkey - sectors password
				serNum - Card serial number, 4 bytes
 * Return values: successful return MI_OK
 */
uchar MFRC522_Auth(uchar authMode, uchar BlockAddr, uchar *Sectorkey, uchar *serNum)
{
    uchar status;
    uint recvBits;
    uchar i;
	uchar buff[12]; 

	// verify command + block address + sectors password + card serial number
    buff[0] = authMode; // verify command
    buff[1] = BlockAddr;
    for(i = 0; i < 6; i++) // sectors password
    {    
		buff[i+2] = *(Sectorkey+i);   
	}
    for(i = 0; i < 4; i++)
    {    
		buff[i+8] = *(serNum+i);   
	}
    status = MFRC522_ToCard(PCD_AUTHENT, buff, 12, buff, &recvBits);
    if((status != MI_OK) || (!(Read_MFRC522(Status2Reg) & 0x08)))
    {   
		status = MI_ERR;   
	}
    
    return status;
}

/*
 * Function: MFRC522_Read
 * Description: Read block data
 * Input parameters: 
 *					blockAddr - block address,
 *					recvData  - read a block data
 * Return values: successful return MI_OK
 */
uchar MFRC522_Read(uchar blockAddr, uchar *recvData)
{
    uchar status;
    uint unLen;

    recvData[0] = PICC_READ;
    recvData[1] = blockAddr;
    CalulateCRC(recvData,2, &recvData[2]);
    status = MFRC522_ToCard(PCD_TRANSCEIVE, recvData, 4, recvData, &unLen);
    if((status != MI_OK) || (unLen != 0x90))
    {
		status = MI_ERR;
    }    
    return status;
}

/*
 * Function: MFRC522_Write
 * Description: Write block data
 * Enter parameters: 
 *					blockAddr - block address,
 *					writeData - write 16 bytes of data to block
 * Return values: successful return MI_OK
 */
uchar MFRC522_Write(uchar blockAddr, uchar *writeData)
{
    uchar status;
    uint recvBits;
    uchar i;
	uchar buff[18]; 
    
    buff[0] = PICC_WRITE;
    buff[1] = blockAddr;
    CalulateCRC(buff, 2, &buff[2]);
    status = MFRC522_ToCard(PCD_TRANSCEIVE, buff, 4, buff, &recvBits);
	if((status != MI_OK) || (recvBits != 4) || ((buff[0] & 0x0F) != 0x0A))
    {
		status = MI_ERR;   
	}   
    if(status == MI_OK)
    {
        for(i = 0; i < 16; i++) // write to the FIFO 16 Byte data
        {
			buff[i] = *(writeData+i);   
        }
        CalulateCRC(buff, 16, &buff[16]);
        status = MFRC522_ToCard(PCD_TRANSCEIVE, buff, 18, buff, &recvBits);
		if((status != MI_OK) || (recvBits != 4) || ((buff[0] & 0x0F) != 0x0A))
        {
			status = MI_ERR;   
		}
    }
    return status;
}

/* command card into hibernation */
void MFRC522_Halt(void)
{
    uint unLen;
    uchar buff[4];
    buff[0] = PICC_HALT;
    buff[1] = 0;
    CalulateCRC(buff, 2, &buff[2]);
	MFRC522_ToCard(PCD_TRANSCEIVE, buff, 4, buff, &unLen);
}

/* MFRC522 Register W/R Test */
void reg_read_write_test(uchar addr, uchar val)
{
	volatile uint8_t test = 0;
	
	//test = Write_MFRC522(addr);
	//printf("Before write, Register 0x%x = 0x%x\n", addr, test);
	Write_MFRC522(addr, val);
	test = Read_MFRC522(addr);
	//printf("After write, Register 0x%x = 0x%x, it should be %x\n", addr, test, val);
	if(test != val)
	{
		puts("MFRC522 register W/R test failed...");
		exit(-1);
	}
	else
		puts("MFRC522 register W/R test successfully!");
}

void card_type_indentify(uint cardTypeID)
{
	switch(cardTypeID)
    {
		case 0x0400: // certification card secret
		{
			puts("Card type: Mifare_One S50");
			break;
		}
		
		case 0x0000:
			// no card, do nothing
			break;
			
		default:
			puts("Card type: Unknown");
			break;
    }
}

/* ----------L298 function---------- */
void L298_init()
{
	pinMode(ENA, OUTPUT);
	pinMode(IN1, OUTPUT);
	pinMode(IN2, OUTPUT);
	digitalWrite(ENA, LOW);
	digitalWrite(IN1, HIGH);
	digitalWrite(IN2, HIGH);
}

void forward(double time)
{
	delay(500);
	digitalWrite(ENA, HIGH);
	digitalWrite(IN2, LOW);
	delay(1000*time);
	digitalWrite(IN2, HIGH);
	slow_stop();
	total_time = total_time + time;
	
}

void reversal(double time)
{
	delay(500);
	digitalWrite(ENA, HIGH);
	digitalWrite(IN1, LOW);
	delay(950*time);
	digitalWrite(IN1, HIGH);
	slow_stop();
	total_time = total_time - time;
	
}

void slow_stop()
{
	digitalWrite(ENA, LOW);
}

void reset_motor()
{
	if(total_time > 0)
	{
		reversal(total_time);
	}
	else if(total_time < 0)
	{
		forward(total_time*(-1));
	}
	else
	{}	// total_time = 0, do nothing
}

/* ----------Database function---------- */
void insert(char *colum1, int value1, char *colum2, int value2, char *colum3, int value3, char *ip, char *port, char *table)
{
	char command[256] = "curl --data ";
	char data[128] = "\"";
	char address[64] = " http://";
	char v1[32], v2[8], v3[8];
	sprintf(v1, "%d", value1);
	sprintf(v2, "%d", value2);
	sprintf(v3, "%d", value3);
	
	strcat(data, colum1);
	strcat(data, "=");
	strcat(data, v1);
	strcat(data, "&");
	strcat(data, colum2);
	strcat(data, "=");
	strcat(data, v2);
	strcat(data, "&");
	strcat(data, colum3);
	strcat(data, "=");
	strcat(data, v3);
	strcat(data, "\"");
	//printf("%s\n", data);
	
	strcat(address, ip);
	strcat(address, ":");
	strcat(address, port);
	strcat(address, "/");
	strcat(address, table);
	//printf("%s\n", address);
	
	strcat(command, data);
	strcat(command, address);
	printf("Execute command: %s\n", command);
	system(command);
}

void retrieval_user_status(char *ip, char *port, char *SN)
{
	char command[256] = "curl -s ";
	char address[64] = "http://";
	
	strcat(address, ip);
	strcat(address, ":");
	strcat(address, port);
	strcat(address, "/");
	strcat(address, "users/");
	strcat(address, SN);
	strcat(address, "/status");
	//printf("%s\n", address);
	
	strcat(command, address);
	strcat(command, " > userStatus.txt");
	printf("Execute command: %s\n", command);
	system(command);
}

int main(int argc, char * argv[])
{
	init(argc, argv);
	puts("");
	
	setup();
	while(1)
	{
		loop();
	}
}
