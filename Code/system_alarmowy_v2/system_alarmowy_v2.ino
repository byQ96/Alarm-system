/********************************************************************************/
// Tytuł projektu: System alarmowy z zamkiem szyfrowym
// Autor: Michał Wieczorek
// Początek projektu: 12/09/2016
// Koniec projektu: --/--/----
// Wersja: 2.0

// Opis:
// System alarmowy bazujący na płytce prototypowej Arduino Uno R3, czujnikach 
// ruchu PIR, wyświetlaczowi LCD, syrenie alarmowej oraz klawiaturze membramowej.
// System na na celu zabezbieczenie budynku: czujniki PIR wysyłają sygnał o ruchu
// który załącza syrenę alarmową. 
// Aktywacja jak i dezaktywacja odbywa się poprzez wpisanie 4 cyfrowego PINU.

// Komponenty:
// Arduino UNO R3
// Czujniki ruchu PIR HC-SR501
// Klawiatura membramowa 4 x 4
// Wyświetlacz LCD 2 x 16 znaków
// Konwerter I2C do wyświetlacza
// Syrena alarmowa?
// 2-kanałowy przekaźnik 5V 10A

// Wersje:
// 1.0 - podstawowa
// 1.1 - dodanie wygaszenia LCD po wciśnięciu przycisku '*'
// 2.0 - dodanie opóźnień (10 sekund po wpisaniu kodu i 10 sekund po uruchomieniu się alarmu)
// [2.1 - dodanie brzęczyka podczas wciskania klawiszy]

/********************************************************************************/
#include <LiquidCrystal_I2C.h> // Bibioteki obsługujące
#include <Wire.h>              // LCD w trybie I2C

#include <Password.h>          // Biblioteki obsługujące
#include <Keypad.h>            // klawiaturę i wpisywanie hasła


// WYŚWIETLACZ LCD 
#define PODSWIETLENIE_LCD 3 // pin cyfrowy 3 przeznaczony jest do sterowania
                            // jasnością ekranu
LiquidCrystal_I2C  lcd (0x27,2,1,0,4,5,6,7); //przekazanie adresu modułu I2R

// KONFIGURACJA HASŁA
const Password haslo = Password ("8161"); //stworzenie stałego obiektu "haslo"

//KONFIGURACJA KLAWIATURY
const byte WIERSZE = 4; // 4 wiersze
const byte KOLUMNY = 4; // 4 kolumny

char klawisze [WIERSZE][KOLUMNY] =   // Tworzenie 
{                                    // mapy
  {'1','2','3','A'},                 // klawiszy
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte PINY_wierszowe [WIERSZE] = {11,10,9,8}; // Deklaracja pinów  
byte PINY_kolumnowe [KOLUMNY] = { 7, 6,5,4}; // pod klawiaturę numeryczną

// Tworzenie obiektu KLAWIATURA
Keypad klawiatura = Keypad (makeKeymap (klawisze), PINY_wierszowe, PINY_kolumnowe,
                            WIERSZE, KOLUMNY);

// Deklaracja pinów pod czujniki ruchu
byte pirPin_1 = 16;  // Pin_anal_A2
byte pirPin_2 = 1;  // Pin_cyf_01
byte pirPin_3 = 2;   // Pin_cyf_02
byte pirPin_4 = 12;  // Pin_cyf_12
byte pirPin_5 = 13;  // Pin_cyf_13

//Deklaracja  pinów pod diody stanu
byte czerwonaLED = 15; // Pin_anal_A1
byte niebieskaLED = 17; // Pin_anal_A3

//Deklaracja pinu pod syrenę alarmową
byte syrena = 14; // Pin_anal_A0

//Zmienne pomocnicze
boolean alarm_aktywny = false;
boolean wykrycie_ruchu = false;
byte strefa = 0;
byte pozycja_kursora = 11;
boolean dioda_LCD = true;
int timer = 0;

/********************************************************************************/
// FUNKCJE

// wyświetla wstępny ekran, który ma pokazać się po uruchomieniu urządzenia
void start_programu ()
{
  // Szybka konfiguracja wyświetlacza
  lcd.begin (16,2);
  lcd.setBacklightPin (PODSWIETLENIE_LCD,POSITIVE);
  lcd.setBacklight (HIGH);

  lcd.home ();
  lcd.print ("System alarmowy");
  lcd.setCursor (0,1);
  lcd.print ("  Wersja: 2.0  ");
  delay (3000);
  lcd.clear ();
  lcd.home ();
  lcd.print ("   Copyrights   ");
  lcd.setCursor (0,1);
  lcd.print ("Michal Wieczorek");
  delay (2000);
  lcd.clear ();
}

//ekran z poziomu którego możeby sterować alarmem (wpisać PIN)
void ekran_podstawowy ()
{
  lcd.clear ();
  
  if (alarm_aktywny == true && wykrycie_ruchu == false)
  {
    lcd.setCursor (0,1);
    lcd.print ("Stan: Aktywny   "); 
  }
  else if (alarm_aktywny == false && wykrycie_ruchu == false)
  {
    lcd.setCursor (0,1);
    lcd.print ("Stan: Nieaktywny");
  }
  else if (alarm_aktywny == true && wykrycie_ruchu == true)
  {
    lcd.setCursor (0,1);
    lcd.print ("Stan: Uruchomiony");
  }
  
  lcd.home ();
  lcd.print ("Podaj PIN:");
  lcd.setCursor (11,0);
  lcd.blink ();
}

// klasyfikacja i nadanie stanów pinom wejściowym i wyjściowym
void klasyfikacja_pinow ()
{
  //klasyfikacja Pinów
  pinMode (czerwonaLED, OUTPUT); // Sygnalizator że alarm jest uzbrojony
  pinMode (syrena, OUTPUT);      // Uzbraja syrenę (przez przekaźnik)

  pinMode (pirPin_1, INPUT);      // 1 czujnik
  pinMode (pirPin_2, INPUT);      // 2 czujnik
  pinMode (pirPin_3, INPUT);      // 3 czujnik
  pinMode (pirPin_4, INPUT);      // 4 czujnik
  pinMode (pirPin_5, INPUT);      // 5 czujnik

  //nadanie stanów
  digitalWrite (czerwonaLED, LOW);
  digitalWrite (niebieskaLED, LOW);
  digitalWrite (syrena, HIGH);
}

// Pobranie pojedynczego klawisza
void pobranie_pinu (KeypadEvent eKlawisz)
{
  switch (klawiatura.getState())
  {
    case PRESSED:
      lcd.setCursor ((pozycja_kursora++), 0);
      switch (eKlawisz)
      {
        case 'A':
        {
          sprawdz_haslo ();
          pozycja_kursora = 11;
          break;
        }
        case 'B':
        {
          resetuj_haslo ();
          pozycja_kursora = 11;
          break;
        }
        case '*':
        {
          if (dioda_LCD == false)
          {
            lcd.setBacklight (HIGH);
            dioda_LCD = true;
          }
          else if (dioda_LCD == true)
          {
            lcd.setBacklight (LOW);
            dioda_LCD = false;
          }
          //resetuj_haslo ();
          pozycja_kursora = 11;
          break;
        }
        default:
        {  
          haslo.append (eKlawisz);
          lcd.print ("*");
        }
      }
     
  }
}

// Sprawdzenie czy hasło jest poprawne
void sprawdz_haslo ()
{
  if (haslo.evaluate())
  { 
    if (alarm_aktywny == false && wykrycie_ruchu == false)
    {
      aktywacja_alarmu ();
    }
    else if (alarm_aktywny == true || wykrycie_ruchu == true)
    {
      deaktywacja_alarmu ();
    }
    
    ekran_podstawowy ();
  }
  else
  {
    niepoprawne_haslo ();
    ekran_podstawowy ();
  }
}

// Resetowanie hasła
void resetuj_haslo ()
{
  haslo.reset();
  ekran_podstawowy ();
}

//Funkcja uruchamiana przy aktywacji alarmu
void aktywacja_alarmu ()
{
  lcd.clear ();
  lcd.home ();
  lcd.print ("     ALARM      ");    // komunikaty na LCD
  lcd.setCursor (0,1);
  lcd.print ("   AKTYWOWANY   ");
  digitalWrite (czerwonaLED, HIGH); // włączenie diody stanu alarmu
  alarm_aktywny = true;             // zmiana stany alarmy na aktywny
  timer = 0;
  haslo.reset ();
  delay (3000); 
}

//Funkcja uruchamiana przy deaktywacji alarmu
void deaktywacja_alarmu ()
{
  lcd.clear ();
  lcd.home ();
  lcd.print ("     ALARM      ");
  lcd.setCursor (0,1);
  lcd.print ("  DEAKTYWOWANY  ");
  digitalWrite (czerwonaLED, LOW); // wyłączenie diody stanu alarmu
  digitalWrite (niebieskaLED, LOW);// wyłączenie diody niebieskiej
  digitalWrite (syrena, HIGH);     // wyłączenie syreny
  alarm_aktywny = false;           // zmana stanu alarmy na nieaktywny
  wykrycie_ruchu = false;          // zresetowanie czujników
  delay (3000);
  haslo.reset ();  
}

// Jeśli podano nieprawidłowe haslo
void niepoprawne_haslo ()
{
  haslo.reset ();
  lcd.clear ();
  lcd.home ();
  lcd.print ("NIEPOPRAWNY PIN!");
  delay (3000);
}

// Jeśli któryś z czujników wykrył ruch
void alarm_zablokowany ()
{ 
  digitalWrite (syrena, LOW);  // uruchomienie sygnału dźwiękowego
  //migajaca_dioda();
  //wykrycie_ruchu = true;
  //alarm_aktywny = false;
  //lcd.clear ();
  lcd.setCursor (0,0);
  lcd.print ("ALARM!    ");

  lcd.setCursor (0,1);
  lcd.print("Czujnik nr: ");
  lcd.setCursor (12,1);

  if (strefa == 1)
  { 
    lcd.print("1   ");
  }
  else if (strefa == 2)
  { 
    lcd.print("2   ");
  }
  else if (strefa == 3)
  { 
    lcd.print("3   ");
  }
  else if (strefa == 4)
  { 
    lcd.print("4   ");
  }
  else if (strefa == 5)
  { 
    lcd.print("5   ");
  }
  lcd.setCursor (11,0);
}

void migajaca_dioda ()
{
  digitalWrite (niebieskaLED, HIGH); //dioda niebieska migająca
  delay (100);
  digitalWrite (niebieskaLED, LOW);
}

/********************************************************************************/
void setup()
{
  start_programu ();

  ekran_podstawowy ();

  klasyfikacja_pinow ();

  klawiatura.addEventListener (pobranie_pinu);
}

void loop() 
{
   klawiatura.getKey(); //jednorazowe "sprawdzenie" klawiatury

   if (alarm_aktywny == true && wykrycie_ruchu == false && timer>=1500)
   {
    if (digitalRead (pirPin_1) == HIGH)
    {
      strefa = 1;
      timer = 0;
      wykrycie_ruchu = true;
      ekran_podstawowy();
    }
    else if (digitalRead (pirPin_2) == HIGH)
    {
      strefa = 2;
      timer = 0;
      wykrycie_ruchu = true;
      ekran_podstawowy();
    }
    else if (digitalRead (pirPin_3) == HIGH)
    {
      strefa = 3;
      timer = 0;
      wykrycie_ruchu = true;
      ekran_podstawowy();
    }
    else if (digitalRead (pirPin_4) == HIGH)
    {
      strefa = 4;
      timer = 0;
      wykrycie_ruchu = true;
      ekran_podstawowy();
    }
    else if (digitalRead (pirPin_5) == HIGH)
    {
      strefa = 5;
      timer = 0;
      wykrycie_ruchu = true;
      ekran_podstawowy();
    }
   }
   
   timer++;
   delay(10);
   
   if (wykrycie_ruchu == true && timer == 1500)
   {
      digitalWrite (niebieskaLED, HIGH);
      alarm_zablokowany();
   }
}
