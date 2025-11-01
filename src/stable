#include <Arduino.h>
#include "HX711.h"
#include "LiquidCrystal_I2C.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"
#include <ArduinoJson.h>

// -------- Configuration --------
#define DT 33
#define SCK 32
HX711 balance;
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define POT_PIN 34 // Potentiomètre pour menu

const char *ssid = "POCO";
const char *password = "12345678";
const String firebasePoidsURL = "https://smartstockbox-default-rtdb.firebaseio.com/poids.json";
const String firebaseProduitURL = "https://smartstockbox-default-rtdb.firebaseio.com/produits.json";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600;
const int daylightOffset_sec = 0;

// -------- Variables --------
float seuilDetection = 10;
float seuilStabilite = 5;
bool poidsEnvoye = false;
bool objetPresent = false;

// -------- Produits --------
struct Produit
{
    String designation;
    float poids_unitaire;
    int qte_par_lot;
};
Produit produits[10];
int totalProduits = 0;
int produitIndex = 0;

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

// Charger les produits depuis Firebase
void chargerProduits()
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    HTTPClient http;
    http.begin(firebaseProduitURL);
    int code = http.GET();

    if (code > 0)
    {
        String payload = http.getString();
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, payload);

        if (!error)
        {
            totalProduits = 0;
            JsonObject obj = doc.as<JsonObject>();
            for (JsonPair kv : obj)
            {
                JsonObject prod = kv.value().as<JsonObject>();
                if (totalProduits < 10)
                {
                    produits[totalProduits].designation = String(prod["designation"].as<const char *>());
                    produits[totalProduits].poids_unitaire = atof(prod["poids_unitaire"].as<const char *>());
                    produits[totalProduits].qte_par_lot = atoi(prod["qte_par_lot"].as<const char *>());
                    totalProduits++;
                }
            }
        }
        else
        {
            Serial.println("Erreur parsing JSON produits");
        }
    }
    else
    {
        Serial.println("Erreur HTTP GET produits");
    }
    http.end();
}

// Envoyer poids et produit à Firebase
void sendDataToFirebase(float poids)
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    HTTPClient http;
    http.begin(firebasePoidsURL);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"poids\":" + String(poids) +
                     ",\"date\":\"" + getTimestamp() +
                     "\",\"zone_prod\":\"TWF\"" +
                     ",\"prod_sortie\":\"" + produits[produitIndex].designation + "\"}";

    int code = http.POST(payload);
    if (code > 0)
        Serial.println("✅ Donnees envoyees!");
    else
        Serial.printf("❌ Erreur envoi : %d\n", code);

    http.end();
}

// -------- Setup --------
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
    lcd.print(WiFi.status() == WL_CONNECTED ? "WiFi Connecte" : "WiFi Echoue");

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    balance.begin(DT, SCK);
    balance.set_scale(299.312);
    balance.tare();

    chargerProduits();

    // Choix initial du produit
    produitIndex = 0;
    lcd.clear();
    lcd.print("Produit choisi:");
    lcd.setCursor(0, 1);
    lcd.print(produits[produitIndex].designation);
    delay(1500);

    lcd.clear();
    lcd.print("Pret a peser...");
}

// -------- Loop --------
void loop()
{
    if (!balance.is_ready())
        return;

    // -------- Menu dynamique --------
    int potValue = analogRead(POT_PIN);
    int newIndex = map(potValue, 0, 4095, 0, totalProduits - 1);

    static int lastMenuIndex = -1;
    static unsigned long lastChangeTime = 0;
    const unsigned long STABLE_DELAY = 1500; // 1,5 s pour confirmer

    // Changement du produit détecté
    if (newIndex != lastMenuIndex)
    {
        lastMenuIndex = newIndex;
        lastChangeTime = millis();
        lcd.clear();
        lcd.print("Choisir produit:");
        lcd.setCursor(0, 1);
        lcd.print(produits[newIndex].designation);
    }

    // Confirmer le produit si stabilisé
    if (millis() - lastChangeTime > STABLE_DELAY)
    {
        if (produitIndex != newIndex)
        {
            produitIndex = newIndex;
            poidsEnvoye = false; // reset pour le nouveau produit
            lcd.clear();
            lcd.print("Produit choisi:");
            lcd.setCursor(0, 1);
            lcd.print(produits[produitIndex].designation);
            delay(1000);
            lcd.clear();
            lcd.print("Pret a peser...");
        }
    }

    // -------- Lecture poids --------
    float poids = abs(balance.get_units(5));

    if (poids < seuilDetection)
    {
        if (objetPresent)
        {
            objetPresent = false;
            poidsEnvoye = false;
            lcd.clear();
            lcd.print("Pret a peser...");
        }
        delay(200);
        return;
    }

    objetPresent = true;

    static float poidsStable = 0;
    if (abs(poids - poidsStable) < seuilStabilite)
    {
        if (!poidsEnvoye)
        {
            poidsEnvoye = true;
            poidsStable = poids;
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
            delay(2000);
            lcd.clear();
            lcd.print("Objet present...");
        }
    }
    else
    {
        poidsStable = poids;
    }

    delay(200);
}
