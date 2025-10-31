#include <Arduino.h>
#include "HX711.h"
#include "LiquidCrystal_I2C.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"

// -------- Configuration --------
#define DT 33
#define SCK 32
HX711 balance;
LiquidCrystal_I2C lcd(0x27, 16, 2);

const char *ssid = "AP_Family";
const char *password = "InfiniteKnowledge4Life?";
const String firebaseURL = "https://smartstockbox-default-rtdb.firebaseio.com/poids.json";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600; // +3h
const int daylightOffset_sec = 0;

// -------- Variables --------
float poidsActuel = 0;
float poidsStable = 0;
float dernierPoidsEnvoye = 0;

float seuilDetection = 10; // Détecter un objet au-dessus de 10g
float seuilStabilite = 5;  // marge de stabilité
unsigned long lastStableTime = 0;
bool poidsEnvoye = false;
bool objetPresent = false;

// -------- Fonctions --------
String getTimestamp()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return "0000-00-00 00:00:00";

    char buf[25];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buf);
}

void sendDataToFirebase(float poids)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("⚠️ WiFi non connecté !");
        return;
    }

    HTTPClient http;
    http.begin(firebaseURL);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"poids\":" + String(poids) +
                     ",\"date\":\"" + getTimestamp() +
                     "\",\"zone_prod\":\"TWF\"" +
                     ",\"prod_sortie\":\"Yaourt Socolait\"}";

    int code = http.POST(payload);
    if (code > 0)
        Serial.println("✅ Données envoyées à Firebase !");
    else
        Serial.printf("❌ Erreur envoi : %d\n", code);

    http.end();
}

void setup()
{
    Serial.begin(115200);
    lcd.init();
    lcd.backlight();

    lcd.print("SmartStock Box");
    lcd.setCursor(0, 1);
    lcd.print("WiFi...");

    WiFi.begin(ssid, password);
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 15)
    {
        delay(500);
        Serial.print(".");
        retry++;
    }

    lcd.clear();
    if (WiFi.status() == WL_CONNECTED)
    {
        lcd.print("WiFi Connecte");
        Serial.println("WiFi connecté !");
    }
    else
    {
        lcd.print("WiFi Echoue");
        Serial.println("WiFi non disponible !");
    }

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    balance.begin(DT, SCK);
    balance.set_scale(299.312);
    balance.tare();

    delay(1500);
    lcd.clear();
    lcd.print("Pret a peser...");
    Serial.println("Pret a peser...");
}

// -------- LOOP --------
void loop()
{
    if (!balance.is_ready())
        return;

    float poids = abs(balance.get_units(5));

    // === ÉTAT 1 : Attente ===
    if (poids < seuilDetection)
    {
        if (objetPresent)
        {
            Serial.println("✅ Objet retiré. Reset balance.");
            objetPresent = false;
            poidsEnvoye = false;
            dernierPoidsEnvoye = 0;

            lcd.clear();
            lcd.print("Pret a peser...");
        }
        delay(200);
        return;
    }

    // === ÉTAT 2 : Objet posé ===
    objetPresent = true;

    // Si le poids se stabilise
    if (abs(poids - poidsStable) < seuilStabilite)
    {
        if (!poidsEnvoye)
        {
            // Poids stable et pas encore envoyé
            poidsEnvoye = true;
            dernierPoidsEnvoye = poids;
            lcd.clear();
            lcd.print("Poids detecte:");
            lcd.setCursor(0, 1);

            if (poids >= 1000)
            {
                lcd.print(poids / 1000, 2);
                lcd.print(" kg");
            }
            else
            {
                lcd.print(poids, 0);
                lcd.print(" g");
            }

            sendDataToFirebase(poids);
            Serial.println("Poids envoye: " + String(poids));
            delay(2000);
            lcd.clear();
            lcd.print("Objet present...");
        }
    }
    else
    {
        // Le poids varie encore
        poidsStable = poids;
    }

    delay(200);
}
