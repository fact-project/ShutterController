/*
 * Web Server - for the FACT lid slow control
 * Need an Ethernet Shield over Arduino.
 *
 * based on the work of
 * Martyn Woerner
 * Alessandro Calzavara, alessandro(dot)calzavara(at)gmail(dot)com
 * and Alberto Capponi, bebbo(at)fast-labs net
 * for Arduino community! :-)
 *
 *
 */

#include <SPI.h>
#include <Ethernet.h>
#include <avr/pgmspace.h>

#include "ShutterController.h"

#define SAMPLES  100

// Each value is the real current value in the motors;
// Define Current Limits in [A] - Offset is 0.5A for no load on the motors
// pushing coefficient ~100 Kg/A
const double _ZeroCurrent         PROGMEM = 0.25; // [A]
const double _CurrentPushingLimit PROGMEM = 0.75; // 0.7-0.5 = 0.2 -> 20  +/- 5 kg
const double _OverCurrent         PROGMEM = 1.50; // 1.5-0.5 = 1   -> 100 +/- 5 kg

const int _StartPoint      =     0;
const int _StartPointLimit =    70;
const int _EndPoint        =  1024;
const int _EndPointLimit   =   775; // This must me 740 for the mockup and >770 for the FACT shutter

// Define Lid Status levels and labels
const int _UNKNOWN        =  0;
const int _CLOSED         =  1;
const int _OPEN           =  2;
const int _STEADY         =  3;
const int _MOVING         =  4;
const int _CLOSING        =  5;
const int _OPENING        =  6;
const int _JAMMED         =  7;
const int _MOTOR_FAULT    =  8;
const int _POWER_PROBLEM  =  9;
const int _OVER_CURRENT   = 10;

const char* _StatusLabel[]  = {"Unknown",
                                      "Closed",
                                      "Open",
                                      "Steady",
                                      "Moving",
                                      "Closing",
                                      "Opening",
                                      "Jammed",
                                      "Motor Fault",
                                      "Power Problem",
                                      "Overcurrent"};
// Define Arduino pins
const int _pinPWM[2]  = {5,   6};
const int _pinDA[2]   = {2,   7};
const int _pinDB[2]   = {3,   8};
//unsigned char _ENDIAG[2]   = {A4, A5};





// Define conversion coefficients
double _ADC2V  = 5. / 1024. ; // ADC channel to Volt
double _V2A    = 0.140;       // 140 mV/A conversion factor for the


// Define sensor value and lid status variables
double  _sensorValue[2]    = {0,0};
double  _currentValue[2]   = {0,0};
uint8_t _LidStatus[2]      = {0,0};

extern int  __bss_end;
extern void *__brkval;

// Http header token delimiters
const char *pSpDelimiters = " \r\n";
const char *pStxDelimiter = "\002";    // STX - ASCII start of text character

/**********************************************************************************************************************
*                                   Strings stored in flash of the HTML we will be transmitting
***********************************************************************************************************************/

// HTTP Request message
const char content_404[] PROGMEM = "HTTP/1.1 404 Not Found\nServer: arduino\nContent-Type: text/html\n\n<html>"
                                         "<head><title>Arduino Web Server - Error 404</title></head>"
                                         "<body><h1>Error 404: Sorry, that page cannot be found!</h1></body>";
// HTML Header for pages
const char content_main_header[] PROGMEM= "HTTP/1.0 200 OK\nServer: arduino\nCache-Control: no-store, no-cache, must-revalidate\n"
                                               "Pragma: no-cache\nConnection: close\nContent-Type: text/html\n";

const char content_main_top[]    PROGMEM   = "<html><head><meta http-equiv=\"refresh\" content=\"5\"/><title>Arduino Web Server</title>"
                                                  "<style type=\"text/css\">table{border-collapse:collapse;}td{padding:0.25em 0.5em;border:0.5em solid #C8C8C8;}</style>"
                                                  "</head><body><h1>Arduino Web Server</h1>";


const char content_main_menu[]   PROGMEM = "<table width=\"500\"><tr><td align=\"center\"><a href=\"/\">Page 1</a></td></tr></table>";
const char content_main_footer[] PROGMEM = "</body></html>";
const char * const contents_main[] PROGMEM = { content_main_header, content_main_top, content_main_menu, content_main_footer }; // table with 404 page

#define CONT_HEADER 0
#define CONT_TOP 1
#define CONT_MENU 2
#define CONT_FOOTER 3


// Page 1
const PROGMEM char http_uri1[] = "/";
const PROGMEM char content_title1[] = "<h2>Shutter Lid Control - Beta</h2>";
const PROGMEM char content_page1[] = "<hr/><form action=\"/__output__\" method=\"POST\">"
                                    "<button name=\"Button5\" value=\"valueButton5\" type=\"submit\">Open Lid</button><p/>"
                                    "<button name=\"Button6\" value=\"valueButton6\" type=\"submit\">Close Lid</button><p/>"
                                    "<span id=\"cur1\" value=\"\002\"> Motor Current 1 = \002 A </span><p/><span id=\"cur2\" value=\"\002\"> Motor Current 2 = \002 A </span><p/>"
                                    "<span id=\"pos1\" value=\"\002\"> Hall Sensor 1 = \002 ADC counts </span><p/><span id=\"pos2\" value=\"\002\"> Hall Sensor 2 = \002 ADC counts</span><p/>"
                                    "</form></p>";
                                    //<br/></body></html>

// Page 5

const PROGMEM char http_uri5[] = "/__output__";
const PROGMEM char content_title5[] = "<h2>Shutter Lid Control - Beta</h2>";
const PROGMEM char content_page5[] = "<hr/><form action=\"/__output__\" method=\"POST\">"
                                    "<button name=\"Button5\" value=\"valueButton5\" type=\"submit\">Open Lid</button><p/>"
                                    "<button name=\"Button6\" value=\"valueButton6\" type=\"submit\">Close Lid</button><p/>"
                                    "<span id=\"cur1\" value=\"\002\"> Motor Current 1 = \002 A </span><p/><span id=\"cur2\" value=\"\002\"> Motor Current 2 = \002 A </span><p/>"
                                    "<span id=\"pos1\" value=\"\002\"> Hall Sensor 1 = \002 ADC ounts </span><p/><span id=\"pos2\" value=\"\002\"> Hall Sensor 2 = \002 ADC counts</span><p/>"
                                    "<span id=\"lid1\" value=\"\002\"> Lid 1 Status : \002</span><p/>"
                                    "<span id=\"lid2\" value=\"\002\"> Lid 2 Status : \002</span><p/>"
                                    "<p/>received a POST request</form><p/>";

// declare tables for the pages
const char * const contents_titles[] PROGMEM = { content_title1,  content_title5 }; // titles
const char * const contents_pages [] PROGMEM = { content_page1,  content_page5 }; // real content

#ifdef USE_IMAGES

/**********************************************************************************************************************
*                     Image strings and data stored in flash for the image UISs we will be transmitting
***********************************************************************************************************************/

// A Favicon is a little custom icon that appears next to a website's URL in the address bar of a web browser.
// They also show up in your bookmarked sites, on the tabs in tabbed browsers, and as the icon for Internet shortcuts
// on your desktop or other folders in Windows.
const PROGMEM char http_uri6[] = "/favicon.ico";    // favicon Request message

const PROGMEM char content_image_header[] = "HTTP/1.1 200 OK\nServer: arduino\nContent-Length: \002\nContent-Type: image/\002\n\r\n";

// declare tables for the images
const char * const image_header PROGMEM = content_image_header;
const char * const data_for_images [] PROGMEM = { content_favicon_data}; // real data
const int   size_for_images [] PROGMEM = { sizeof(content_favicon_data)};
// declare table for all URIs
const char * const http_uris[] PROGMEM = { http_uri1, http_uri5, http_uri6 }; // URIs


#else
const char * const image_header PROGMEM = NULL;
const char * const data_for_images [] PROGMEM = { }; // real data
const int   size_for_images [] PROGMEM = { };
// declare table for all URIs
const char * const http_uris[] PROGMEM = { http_uri1, http_uri5 }; // URIs

#endif  // USE_IMAGES

#define NUM_PAGES  sizeof(contents_pages)  / sizeof(const char * const)
#define NUM_IMAGES sizeof(data_for_images) / sizeof(const char * const)    // favicon or png format
#define NUM_URIS  NUM_PAGES + NUM_IMAGES  // Pages URIs + favicon URI, etc

/**********************************************************************************************************************
*                                                 Shared variable and Setup()
***********************************************************************************************************************/
EthernetServer server(80);

MethodType readHttpRequest(EthernetClient & client, int & nUriIndex, BUFFER & requestContent);
void sendProgMemAsString(EthernetClient & client, const char *realword);
void sendPage(EthernetClient & client, int nUriIndex, BUFFER & requestContent);
void sendImage(EthernetClient & client, int nUriIndex, BUFFER & requestContent);
MethodType readRequestLine(EthernetClient & client, BUFFER & readBuffer, int & nUriIndex, BUFFER & requestContent);
void readRequestHeaders(EthernetClient & client, BUFFER & readBuffer, int & nContentLength, bool & bIsUrlEncoded);
void readEntityBody(EthernetClient & client, int nContentLength, BUFFER & content);
void getNextHttpLine(EthernetClient & client, BUFFER & readBuffer);
int GetUriIndex(char * pUri);
void sendSubstitute(EthernetClient client, int nUriIndex, int nSubstituteIndex, BUFFER & requestContent);
double ReadCurrentM(int motor,  int samples);
void MoveTo(int motor, double target_position, int mySpeed);
void sendUriContentByIndex(EthernetClient client, int nUriIndex, BUFFER & requestContent);
void sendProgMemAsBinary(EthernetClient & client, const char* realword, int realLen);
int availableMemory();

byte _mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x5C, 0x91 };
IPAddress _ip(10, 0, 100, 36);

#include "DualVNH5019MotorShield.h"
#include "LinakHallSensor.h"

const int M1INA = 2;
const int M2INA = 7;
const int M1INB = 3;
const int M2INB = 8;

DualVNH5019MotorShield md(
    M1INA, // INA1 : "M1INA"
    M1INB, // INB1 : "M1INB"
    A0, // CS1: "M1CS"
    M2INA, // INA2 : "M2INA" - original 7
    M2INB, // INB2 : "M2INB" - original 8
    A1 // CS2 : "M2CS"
    );

LinakHallSensor lh(A2, A3);

void setup()
{
    Serial.begin(115200);
    Serial.println("Using hard-coded ip...");
    Ethernet.begin(_mac, _ip);

  Serial.print("My IP address is ");
  Serial.println(Ethernet.localIP());

  server.begin();

  md.init();
  lh.init();
}

/**********************************************************************************************************************
*                                                           Main loop
***********************************************************************************************************************/

void loop()
{
  EthernetClient client = server.available();
  if (client)
  {
      // now client is connected to arduino we need to extract the
    // following fields from the HTTP request.
    int    nUriIndex;  // Gives the index into table of recognized URIs or -1 for not found.
    BUFFER requestContent;    // Request content as a null-terminated string.
    MethodType eMethod = readHttpRequest(client, nUriIndex, requestContent);

    if (nUriIndex < 0)
    {
      // URI not found
      sendProgMemAsString(client, (char*)pgm_read_word(&content_404));
    }
    else if (nUriIndex < NUM_PAGES)
    {
      // Normal page request, may depend on content of the request
      sendPage(client, nUriIndex, requestContent);
    }
    else
    {
      // Image request
      sendImage(client, nUriIndex, requestContent);
    }

    // give the web browser time to receive the data
    //  delay(1);
    delay(100);

    client.stop();
  }
}

/**********************************************************************************************************************
*                                              Method for read HTTP Header Request from web client
*
* The HTTP request format is defined at http://www.w3.org/Protocols/HTTP/1.0/spec.html#Message-Types
* and shows the following structure:
*  Full-Request and Full-Response use the generic message format of RFC 822 [7] for transferring entities. Both messages may include optional header fields
*  (also known as "headers") and an entity body. The entity body is separated from the headers by a null line (i.e., a line with nothing preceding the CRLF).
*      Full-Request   = Request-Line
*                       *( General-Header
*                        | Request-Header
*                        | Entity-Header )
*                       CRLF
*                       [ Entity-Body ]
*
* The Request-Line begins with a method token, followed by the Request-URI and the protocol version, and ending with CRLF. The elements are separated by SP characters.
* No CR or LF are allowed except in the final CRLF sequence.
*      Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
* HTTP header fields, which include General-Header, Request-Header, Response-Header, and Entity-Header fields, follow the same generic format.
* Each header field consists of a name followed immediately by a colon (":"), a single space (SP) character, and the field value.
* Field names are case-insensitive. Header fields can be extended over multiple lines by preceding each extra line with at least one SP or HT, though this is not recommended.
*      HTTP-header    = field-name ":" [ field-value ] CRLF
***********************************************************************************************************************/
// Read HTTP request, setting Uri Index, the requestContent and returning the method type.
MethodType readHttpRequest(EthernetClient & client, int & nUriIndex, BUFFER & requestContent)
{


  BUFFER readBuffer;    // Just a work buffer into which we can read records
  int nContentLength = 0;
  bool bIsUrlEncoded;

  requestContent[0] = 0;    // Initialize as an empty string
  // Read the first line: Request-Line setting Uri Index and returning the method type.
  MethodType eMethod = readRequestLine(client, readBuffer, nUriIndex, requestContent);
  // Read any following, non-empty headers setting content length.
  readRequestHeaders(client, readBuffer, nContentLength, bIsUrlEncoded);

  if (nContentLength > 0)
  {
  // If there is some content then read it and do an elementary decode.
    readEntityBody(client, nContentLength, requestContent);
    if (bIsUrlEncoded)
    {
    // The '+' encodes for a space, so decode it within the string
    for (char * pChar = requestContent; (pChar = strchr(pChar, '+')) != NULL; )
      *pChar = ' ';    // Found a '+' so replace with a space
    }
  }

  return eMethod;
}

// Read the first line of the HTTP request, setting Uri Index and returning the method type.
// If it is a GET method then we set the requestContent to whatever follows the '?'. For a other
// methods there is no content except it may get set later, after the headers for a POST method.
MethodType readRequestLine(EthernetClient & client, BUFFER & readBuffer, int & nUriIndex, BUFFER & requestContent)
{
  MethodType eMethod;
  // Get first line of request:
  // Request-Line = Method SP Request-URI SP HTTP-Version CRLF
  getNextHttpLine(client, readBuffer);
  // Split it into the 3 tokens
  char * pMethod  = strtok(readBuffer, pSpDelimiters);
  char * pUri     = strtok(NULL, pSpDelimiters);
  char * pVersion = strtok(NULL, pSpDelimiters);
  // URI may optionally comprise the URI of a queryable object a '?' and a query
  // see http://www.ietf.org/rfc/rfc1630.txt
  strtok(pUri, "?");
  char * pQuery   = strtok(NULL, "?");
  if (pQuery != NULL)
  {
    strcpy(requestContent, pQuery);
    // The '+' encodes for a space, so decode it within the string
    for (pQuery = requestContent; (pQuery = strchr(pQuery, '+')) != NULL; )
      *pQuery = ' ';    // Found a '+' so replace with a space

//    Serial.print("Get query string: ");
//    Serial.println(requestContent);
  }
  if (strcmp(pMethod, "GET") == 0){
    eMethod = MethodGet;
}
  else if (strcmp(pMethod, "POST") == 0){
    eMethod = MethodPost;
}
  else if (strcmp(pMethod, "HEAD") == 0){
    eMethod = MethodHead;
  }
  else
    eMethod = MethodUnknown;

  // See if we recognize the URI and get its index
  nUriIndex = GetUriIndex(pUri);

  return eMethod;
}

// Read each header of the request till we get the terminating CRLF
void readRequestHeaders(EthernetClient & client, BUFFER & readBuffer, int & nContentLength, bool & bIsUrlEncoded)
{
  nContentLength = 0;      // Default is zero in cate there is no content length.
  bIsUrlEncoded  = true;   // Default encoding
  // Read various headers, each terminated by CRLF.
  // The CRLF gets removed and the buffer holds each header as a string.
  // An empty header of zero length terminates the list.
  do
  {
    getNextHttpLine(client, readBuffer);
    // Process a header. We only need to extract the (optionl) content
    // length for the binary content that follows all these headers.
    // General-Header = Date | Pragma
    // Request-Header = Authorization | From | If-Modified-Since | Referer | User-Agent
    // Entity-Header  = Allow | Content-Encoding | Content-Length | Content-Type
    //                | Expires | Last-Modified | extension-header
    // extension-header = HTTP-header
    //       HTTP-header    = field-name ":" [ field-value ] CRLF
    //       field-name     = token
    //       field-value    = *( field-content | LWS )
    //       field-content  = <the OCTETs making up the field-value
    //                        and consisting of either *TEXT or combinations
    //                        of token, tspecials, and quoted-string>
    char * pFieldName  = strtok(readBuffer, pSpDelimiters);
    char * pFieldValue = strtok(NULL, pSpDelimiters);

    if (strcmp(pFieldName, "Content-Length:") == 0)
    {
      nContentLength = atoi(pFieldValue);
    }
    else if (strcmp(pFieldName, "Content-Type:") == 0)
    {
      if (strcmp(pFieldValue, "application/x-www-form-urlencoded") != 0)
        bIsUrlEncoded = false;
    }
  } while (strlen(readBuffer) > 0);    // empty string terminates
}

// Read the entity body of given length (after all the headers) into the buffer.
void readEntityBody(EthernetClient & client, int nContentLength, BUFFER & content)
{
  int i;
  char c;

  if (nContentLength >= sizeof(content))
    nContentLength = sizeof(content) - 1;  // Should never happen!

  for (i = 0; i < nContentLength; ++i)
  {
    c = client.read();
    content[i] = c;
  }

  content[nContentLength] = 0;  // Null string terminator

//  Serial.print("Content: ");
//  Serial.println(content);
}

// See if we recognize the URI and get its index; or -1 if we don't recognize it.
int GetUriIndex(char * pUri)
{

  int nUriIndex = -1;
  // select the page from the buffer (GET and POST) [start]
  for (int i = 0; i < NUM_URIS; i++)
  {
    if (strcmp_P(pUri, (const char * const)pgm_read_word(&(http_uris[i]))) == 0)
    {
      nUriIndex = i;

      break;
    }
  }
//  Serial.print("URI: ");
//  Serial.print(pUri);
//  Serial.print(" Page: ");
//  Serial.println(nUriIndex);

  return nUriIndex;
}

/**********************************************************************************************************************
* Read the next HTTP header record which is CRLF delimited.  We replace CRLF with string terminating null.
***********************************************************************************************************************/
void getNextHttpLine(EthernetClient & client, BUFFER & readBuffer)
{
  char c;
  int bufindex = 0; // reset buffer

  // reading next header of HTTP request
  if (client.connected() && client.available())
  {
    // read a line terminated by CRLF
    readBuffer[0] = client.read();
    readBuffer[1] = client.read();
    bufindex = 2;
    for (int i = 2; readBuffer[i - 2] != '\r' && readBuffer[i - 1] != '\n'; ++i)
    {
      // read full line and save it in buffer, up to the buffer size
      c = client.read();
      if (bufindex < sizeof(readBuffer))
        readBuffer[bufindex++] = c;
    }
    readBuffer[bufindex - 2] = 0;  // Null string terminator overwrites '\r'
  }
}

/**********************************************************************************************************************
*                                                              Send Pages
       Full-Response  = Status-Line
                        *( General-Header
                         | Response-Header
                         | Entity-Header )
                        CRLF
                        [ Entity-Body ]

       Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
       General-Header = Date | Pragma
       Response-Header = Location | Server | WWW-Authenticate
       Entity-Header  = Allow | Content-Encoding | Content-Length | Content-Type
                      | Expires | Last-Modified | extension-header
*
***********************************************************************************************************************/
void sendPage(EthernetClient & client, int nUriIndex, BUFFER & requestContent)
{
  // Read Sensor values for every page reload
  _sensorValue[0] = lh.get_mean(0, 100);
  _sensorValue[1] = lh.get_mean(1, 100);
  ReadCurrentM(-1,100);

  if (strncmp(requestContent, "Button1=", 8) == 0){
    //Action1(strncmp(&requestContent[9], "true", 4) == 0);
    MoveTo(0, _EndPoint, 255);
  }
  else if (strncmp(requestContent, "Button2=", 8) == 0){
    //Action2(strncmp(&requestContent[9], "true", 4) == 0);
    MoveTo(0, _StartPoint, 255);
  }
  else if (strncmp(requestContent, "Button3=", 8) == 0){
    //Action3(strncmp(&requestContent[9], "true", 4) == 0);
    MoveTo(1, _EndPoint, 255);
  }
  else if (strncmp(requestContent, "Button4=", 8) == 0){
    //Action4(strncmp(&requestContent[9], "true", 4) == 0);
    MoveTo(1, _StartPoint, 255);
  }
  else if (strncmp(requestContent, "Button5=", 8) == 0){
    MoveTo(1, _StartPoint, 255);
    delay(100);
    MoveTo(0, _StartPoint, 255);
  }
  else if (strncmp(requestContent, "Button6=", 8) == 0){
    MoveTo(0, _EndPoint, 255);
    delay(100);
    MoveTo(1, _EndPoint, 255);
  }

  // send HTML header
  // sendProgMemAsString(client,(char*)pgm_read_word(&(contents_main[CONT_HEADER])));
  sendProgMemAsString(client, (char*)pgm_read_word(&(contents_main[CONT_TOP])));

  // send menu
  sendProgMemAsString(client, (char*)pgm_read_word(&(contents_main[CONT_MENU])));

  // send title
  sendProgMemAsString(client, (char*)pgm_read_word(&(contents_titles[nUriIndex])));

  // send the body for the requested page
  sendUriContentByIndex(client, nUriIndex, requestContent);

  // Append the data sent in the original HTTP request
  client.print("<br/>");
  // send POST variables
  client.print(requestContent);

  // send footer
  sendProgMemAsString(client,(char*)pgm_read_word(&(contents_main[CONT_FOOTER])));
}

/**********************************************************************************************************************
*                                                              Send Images
***********************************************************************************************************************/
void sendImage(EthernetClient & client, int nUriIndex, BUFFER & requestContent)
{
  int nImageIndex = nUriIndex - NUM_PAGES;

  // send the header for the requested image
  sendUriContentByIndex(client, nUriIndex, requestContent);

  // send the image data
  sendProgMemAsBinary(client, (char *)pgm_read_word(&(data_for_images[nImageIndex])), (int)pgm_read_word(&(size_for_images[nImageIndex])));
}

/**********************************************************************************************************************
*                                                              Send content split by buffer size
***********************************************************************************************************************/
// If we provide string data then we don't need specify an explicit size and can do a string copy
void sendProgMemAsString(EthernetClient & client, const char *realword)
{
  sendProgMemAsBinary(client, realword, strlen_P(realword));
}

// Non-string data needs to provide an explicit size
void sendProgMemAsBinary(EthernetClient & client, const char* realword, int realLen)
{
  int remaining = realLen;
  const char * offsetPtr = realword;
  int nSize = sizeof(BUFFER);
  BUFFER buffer;

  while (remaining > 0)
  {
    // print content
    if (nSize > remaining)
      nSize = remaining;      // Partial buffer left to send

    memcpy_P(buffer, offsetPtr, nSize);

    if (client.write((const uint8_t *)buffer, nSize) != nSize)
      Serial.println("Failed to send data");

    // more content to print?
    remaining -= nSize;
    offsetPtr += nSize;
  }
}

/**********************************************************************************************************************
*                                                              Send real page content
***********************************************************************************************************************/
// This method takes the contents page identified by nUriIndex, divides it up into buffer-sized
// strings, passes it on for STX substitution and finally sending to the client.
void sendUriContentByIndex(EthernetClient client, int nUriIndex, BUFFER & requestContent)
{
  // Locate the page data for the URI and prepare to process in buffer-sized chunks.
  const char * offsetPtr;               // Pointer to offset within URI for data to be copied to buffer and sent.
  char *pNextString;
  int nSubstituteIndex = -1;            // Count of substitutions so far for this URI
  int remaining;                        // Total bytes (of URI) remaining to be sent
  int nSize = sizeof(BUFFER) - 1;       // Effective size of buffer allowing last char as string terminator
  BUFFER buffer;

  if (nUriIndex < NUM_PAGES)
    offsetPtr = (char*)pgm_read_word(&(contents_pages[nUriIndex]));
  else
    offsetPtr = (char*)pgm_read_word(&(image_header));

  buffer[nSize] = 0;  // ensure there is always a string terminator
  remaining = strlen_P(offsetPtr);  // Set total bytes of URI remaining

  while (remaining > 0)
  {
    // print content
    if (nSize > remaining)
    {
      // Set whole buffer to string terminator before copying remainder.
      memset(buffer, 0, STRING_BUFFER_SIZE);
      nSize = remaining;      // Partial buffer left to send
    }
    memcpy_P(buffer, offsetPtr, nSize);
    offsetPtr += nSize;
    // We have a buffer's worth of page to check for substitution markers/delimiters.
    // Scan the buffer for markers, dividing it up into separate strings.
    if (buffer[0] == *pStxDelimiter)    // First char is delimiter
    {
      sendSubstitute(client, nUriIndex, ++nSubstituteIndex, requestContent);
      --remaining;
    }
    // First string is either terminated by the null at the end of the buffer
    // or by a substitution delimiter.  So simply send it to the client.
    pNextString = strtok(buffer, pStxDelimiter);
    client.print(pNextString);
    remaining -= strlen(pNextString);
    // Scan for strings between delimiters
    for (pNextString = strtok(NULL, pStxDelimiter); pNextString != NULL && remaining > 0; pNextString = strtok(NULL, pStxDelimiter))
    {
      // pNextString is pointing to the next string AFTER a delimiter
      sendSubstitute(client, nUriIndex, ++nSubstituteIndex, requestContent);
      --remaining;
      client.print(pNextString);
      remaining -= strlen(pNextString);
    }
  }
}

// Call this method in response to finding a substitution character '\002' within some
// URI content to send the appropriate replacement text, depending on the URI index and
// the substitution index within the content.
void sendSubstitute(EthernetClient client, int nUriIndex, int nSubstituteIndex, BUFFER & requestContent)
{
  if (nUriIndex < NUM_PAGES)
  {
    // Page request
    switch (nUriIndex)
    {
      case 1:  // page 2
        if (nSubstituteIndex < 4){
          //client.print("<b>");
          client.print(_currentValue[nSubstituteIndex/2]);
          //client.print("</b>");
        }
        else if ( (nSubstituteIndex >= 4) &&
                  (nSubstituteIndex <  8)     ) {
          //client.print("<b>");
          client.print(_sensorValue[nSubstituteIndex/2-2]);
          //client.print("</b>");
        }
        else if ( (nSubstituteIndex >= 8) &&
                  (nSubstituteIndex <  12)     ) {
          //client.print("<b>");
          client.print(_StatusLabel[_LidStatus[nSubstituteIndex/2-4]]);
          //client.print("</b>");

        }
        break;
      case 2:  // page 3
        Serial.println(" -> Case 2");
        break;
    }
  }
  else
  {
    // Image request
    int nImageIndex = nUriIndex - NUM_PAGES;

    switch (nSubstituteIndex)
    {
      case 0:
        // Content-Length value - ie. image size
        char strSize[6];  // Up to 5 digits plus null terminator
        itoa((int)pgm_read_word(&(size_for_images[nImageIndex])), strSize, 10);
        //Serial.println(strSize);    // Debug
        client.print(strSize);
        break;
      case 1:
        // Content-Type partial value
        switch (nImageIndex)
        {
          case 0:  // favicon
            client.print("x-icon");
            break;
          case 1:  // led on image
          case 2:  // led off image
            client.print("png");
            break;
        }
    }
  }
}


void MoveTo(int motor, double target_position, int mySpeed){

  // define tmp value for the speed
  int speedTmp = 0;

  // define variable containing the current actuator position
  // the travel to be done to rech the target position and the
  // motor current
  double current_position;
  double err_current_position;

  double original_position;
  double err_original_position;

  double motor_current;
  double err_motor_current;

  double travel;

  // only one reading to define the position is not sufficient
  // current_position = lh.get(motor);

  // Calculate average final position
  double tmp=0;
  double tmpM=0;
  double tmpS=0;
  int    steps=0;

  for (int i=0;i<SAMPLES;i++){

    tmp=lh.get(motor);
    tmpM += tmp;
    tmpS += tmp*tmp;
  }
  tmpM /= SAMPLES;
  tmpS  = sqrt(tmpS/SAMPLES - tmpM*tmpM);


  int tmpS_int = (int) tmpS;

  // round the mean to it to the closest integer
  current_position = (int) (tmpM+0.5);
  if (((int)tmpS) < 1){
    err_current_position =     1.;
  }
  else{
    err_current_position = tmpS;
  }

  original_position      = current_position;
  err_original_position  = err_current_position;

  // calculate the travel needed to reach the target position
  travel = target_position - current_position;

  Serial.print("Moving motor ");
  Serial.print(motor);
  Serial.print(" from pos. ");
  Serial.print(current_position,3);
  Serial.print(" to position ");
  Serial.print(target_position,3);
  Serial.print(" - Travel distance = ");
  Serial.println(travel,3);

  Serial.print(" - The current position of the actuator is ");
  Serial.print(tmpM,3);
  Serial.print( " +/- " );
  Serial.println(tmpS,3);

  Serial.print(" - The corrected position of the actuator is ");
  Serial.print(current_position);
  Serial.print( " +/- " );
  Serial.println(err_current_position);


  // [IF] the travel is bigger than +2*(absolute position error) try to move out the motor
  if (travel > 2*err_current_position){
    // Try to place here (if you have time) a speed self adjucting algorithm
    // base on the trave which the actuator has still to do
    if( _LidStatus[motor] != _CLOSED){

      Serial.println(" - going out (lid closing)");
      speedTmp = mySpeed; // positive speed
      steps++;
      _LidStatus[motor] = _CLOSING;

      // Accelerate motor from 0 to speedTmp linearly
      for (int j=0;j<speedTmp;j++){
        md.setMotorSpeed(motor, j);
        delay(1);
      }
    }
    else{
      Serial.println(" - already closed");

      _LidStatus[motor] = _CLOSED;
      return;
    }
  }
  // [ELSE IF] travel is between -2*(absolute position error) and +2*(absolute position error)
  //           consider yourself already in position
  else if (travel <=  2*err_current_position &&
           travel >= -2*err_current_position    ){
    Serial.println(" - already in place");

    _LidStatus[motor] = _STEADY;
    return;
    // already in place don't bother moving more.
  }
  // [ELSE} if the travel is smaller than -2*(absolute position error) try to move in the motor
  else{
    Serial.println(" - going in (lid opening)");
    speedTmp = -mySpeed; // negative speed
    steps++;
    _LidStatus[motor] = _OPENING;

    // Accelerate motor from 0 to -speedTmp linearly
    for (int j=0;j>speedTmp;j--){
      md.setMotorSpeed(motor, j);
      delay(1);
    }
  }


  // Start the main loop which checks the motors while they are mooving
  tmp=0;
  tmpM=0;
  tmpS=0;
  for (steps=1;abs(travel) != 0 ;steps++){
    //Read Current
    motor_current = ReadCurrentM(motor,10);

    // If Overcurrent stop
    if (motor_current > _OverCurrent){
      md.setMotorSpeed(motor, 0);

      Serial.print(" - WARNING!!! Overcurrent ");
      Serial.print(motor_current,3);
      Serial.print(" [A] at position ");
      Serial.println(current_position,3);

      _LidStatus[motor] = _OVER_CURRENT;
      return;
    }

    // Average Current around the steps
    tmp   =  motor_current;
    tmpM += tmp;
    tmpS += tmp*tmp;

    // Read current position
    // it doesn't make sense to read it here more time as the actuars are moving
    _sensorValue[motor] = lh.get_mean(motor, 10);
    current_position = _sensorValue[motor];

    // Calculate travel distance
    travel           = target_position - current_position;

    // Read current absorbed the motor and append the values for the calculation
    // of the mean value and its error


    // [IF] the current drops below ~0.07 A might means that the end swirch
    //      stopped the motor check also the position to determine if this is true.
    if (motor_current < _ZeroCurrent){
      // Closing
      if ( current_position > _EndPointLimit && target_position > _EndPointLimit ){
        Serial.print(" - Reached End of Actuator. Pos = ");
        Serial.println(current_position, 3);

        _LidStatus[motor] = _CLOSED;
        break; //Exit from the for loop
      }
      // Opening
      else if (current_position < _StartPointLimit && target_position < _StartPointLimit ){
        Serial.print(" - Reached Beginning of Actuator. Pos = ");
        Serial.println(current_position, 3);

        _LidStatus[motor] = _OPEN;
        break; //Exit from the for loop
      }
      // Error
      else  {
        Serial.print(" - Error!! No current in motor ");
        Serial.print(motor);
        Serial.print(". I= ");
        Serial.print(motor_current);
        Serial.print("A . Pos = ");
        Serial.println(current_position, 3);

        _LidStatus[motor]  = _POWER_PROBLEM;
        break; //Exit from the for loop
      }
    }

   if (_LidStatus[motor] == _CLOSING && motor_current > _CurrentPushingLimit && current_position > _EndPointLimit){
      Serial.print(" - step ");
      Serial.print(steps);
      // double travel_done= (55. * (target_position - original_position))/1024.;
      // Serial.print(" - travelled for ");
      // Serial.print(travel_done);
      Serial.print(" - pushing -> I = ");
      Serial.print(motor_current, 3);
      Serial.println(" A");
      _LidStatus[motor] = _CLOSED;
      break;
    }

    // If too many cycles of the loop print a message and check the position and the current
    if (steps %50 == 0){
      Serial.print(" - step ");
      Serial.print(steps);
      Serial.print(" - still in loop. Pos = ");
      Serial.print(current_position, 3);
      Serial.print(" - Istantaneous current = ");
      Serial.print(motor_current, 3);
      Serial.print(" A");
      Serial.print(" - Average current = ");
      Serial.print(tmpM/steps, 3);
      Serial.println(" A");


      if( _LidStatus[motor] != _CLOSING &&
          _LidStatus[motor] != _OPENING)
        _LidStatus[motor] = _MOVING;

      // Redefine better those limits... they might not be necessary with the new low
      // current limits
      //if (steps %500 && current_position > 1000 && target_position > 1000 ){
      //  Serial.print(" - Reached End of Actuator. Pos = ");
      //  Serial.println(current_position, 3);
      //  break;
      //}
      //if (steps %500 && current_position < 80 && target_position < 80 ){
      //  Serial.print(" - Reached Beginning of Actuator. Pos = ");
      //  Serial.println(current_position, 3);
      //  break;
      //}
    }



    // minimum delay between a cycle and the other is 1 ms
    delay (10);
  }

  // At this stage the motor should be stopped in any case
  md.setMotorSpeed(motor, 0);
  Serial.print(" - motor reached the destination - pos. ");
  Serial.println(current_position,3);

  // Calculate current average and sigma
  tmpM /= steps;
  tmpS  = sqrt(tmpS/steps - tmpM*tmpM);
  Serial.print(" - average current over the loop ");
  Serial.print(tmpM,3);
  Serial.print( " +/- " );
  Serial.println(tmpS,3);

  // Wait 100 ms then calculate average final position
  delay(100);
  tmp=0; tmpM=0; tmpS=0;

  for (int i=0;i<SAMPLES;i++){
    tmp=lh.get(motor);
    tmpM += tmp;
    tmpS += tmp*tmp;
  }
  tmpM /= SAMPLES;
  tmpS  = sqrt(tmpS/SAMPLES - tmpM*tmpM);

  Serial.print(" - final position is ");
  Serial.print(tmpM,3);
  Serial.print( " +/- " );
  Serial.println(tmpS,3);

  Serial.print(" - Lid staus is ");
  Serial.println(_StatusLabel[_LidStatus[motor]]);

  Serial.print("AvailableMemory()=");
  Serial.println(availableMemory());


}

int availableMemory() {
  int free_memory;
  if ((int)__brkval==0)
    free_memory = ((int)&free_memory) - ((int) &__bss_end);
  else
    free_memory = ((int)&free_memory) - ((int) &__brkval);

  return free_memory;
}

double ReadCurrentM(int motor,  int samples){
  switch (motor){
    case -1: // Read all of them
      _currentValue[0] = 0;
      _currentValue[1] = 0;
      for (int j=0;j<samples;j++){
        _currentValue[0] +=  analogRead(A4)*_ADC2V/(_V2A*10.);
        _currentValue[1] +=  analogRead(A5)*_ADC2V/(_V2A*10.);
      }
      _currentValue[0]/=samples;
      _currentValue[1]/=samples;
      return -1;
    case 0:
      _currentValue[motor] = 0;
      for (int j=0;j<samples;j++)
        _currentValue[motor] +=  analogRead(A4)*_ADC2V/(_V2A*10.);
      _currentValue[motor]/=samples;
      return _currentValue[motor];
    case 1:
      _currentValue[motor] = 0;
      for (int j=0;j<samples;j++)
        _currentValue[motor] +=  analogRead(A5)*_ADC2V/(_V2A*10.);
      _currentValue[motor]/=samples;
      return _currentValue[motor];

  }

}