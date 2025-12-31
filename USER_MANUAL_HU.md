# Teensy 4 Art-Net LED Vezérlő - Használati Útmutató

## Gyors Üzembe Helyezés

### Rendszer Áttekintés
A Teensy 4 Art-Net LED Vezérlő egy professzionális eszköz, amely Art-Net adatokat fogad Ethernet-en keresztül és WS281x LED szalagokat vezérel. Web-alapú konfigurációs felülettel rendelkezik és több független kimenetet támogat.

### Alapvető Specifikációk
- **Támogatott LED-ek:** WS2811, WS2812, WS2812B, WS2813, SK6812
- **Maximum LED kimenetenként:** 1020 pixel
- **Elérhető verziók:** 8, 16 vagy 32 független kimenet
- **Hálózat:** 10/100 Ethernet Art-Net protokollal
- **Konfiguráció:** Web-alapú felület
- **Tápellátás:** 5V USB-n vagy külső tápegységen keresztül

## Kezdeti Beállítás

### 1. Hardver Csatlakoztatás
1. Csatlakoztassa a Teensy 4.1-et a hálózathoz Ethernet kábellel
2. Csatlakoztassa a LED szalagokat a kijelölt kimeneti pinekhez (lásd pin kiosztás)
3. Csatlakoztasson nyomógombot a 30-as pinhez (opcionális, teszt módokhoz)
4. Táplálja a Teensy-t USB-n vagy külső 5V tápegységgel

### 2. Hálózati Konfiguráció
**Alapértelmezett Hálózati Beállítások:**
- **IP Cím:** 10.10.0.42
- **Alhálózati Maszk:** 255.255.255.0
- **MAC Cím:** 04:E5:E9:00:69:EC

### 3. Web Felület Elérése
1. Csatlakoztassa számítógépét ugyanarra a hálózatra
2. Nyissa meg a böngészőt és navigáljon a címre: `http://10.10.0.42`
3. A konfigurációs oldal automatikusan betöltődik

## Pin Kiosztás

### 8 Kimenetes Verzió
- **1-8 Kimenetek:** 2, 3, 4, 5, 6, 7, 8, 9 pinek

### 16 Kimenetes Verzió
- **1-8 Kimenetek:** 2, 3, 4, 5, 6, 7, 8, 9 pinek
- **9-16 Kimenetek:** 21, 20, 19, 18, 17, 16, 15, 14 pinek

### 32 Kimenetes Verzió
- **1-32 Kimenetek:** Bővített pin kiosztás (lásd fejlesztői dokumentáció)

## Konfiguráció

### Kimenet Konfiguráció
Minden kimenet függetlenül konfigurálható:

**Pixelek Száma (1-1020)**
- Állítsa be az egyes kimenetekhez csatlakoztatott LED-ek számát
- Maximum 1020 pixel kimenetenként memória korlátozások miatt

**Start Universe (0-999)**
- Art-Net univerzum, ahol ez a kimenet megkezdi az adatok fogadását
- Alapértelmezett távolság: 6 univerzum 1020 pixeles kimenetenként

**Start Address (1-510)**
- DMX csatorna a start univerzumon belül, ahol az adatok kezdődnek
- Standard DMX univerzum 512 csatornával rendelkezik (510 használható RGB-hez)

**Fényerő (0-255)**
- Egyedi fényerő szabályozás kimenetenként
- 255 = 100% fényerő, 0 = kikapcsolva

### LED Konfiguráció
**LED Típus:**
- WS2811
- WS2812/WS2812B
- WS2813
- SK6812

**Szín Sorrend:**
- RGB, GRB, BRG, RBG, GBR, BGR
- WRGB (RGBW LED-ekhez)

### Hálózati Beállítások
**IP Cím:** Statikus IP beállítása a vezérlőhöz
**Alhálózati Maszk:** Hálózati alhálózat konfiguráció
**MAC Cím:** Egyedi hálózati azonosító

## Teszt Módok

### Web Felületi Teszt
Használja a legördülő menüt teszt minták kiválasztásához:
- **Rainbow:** Animált szivárvány effekt
- **Red:** Egyszínű vörös
- **Green:** Egyszínű zöld
- **Blue:** Egyszínű kék

### Gomb Vezérlésű Teszt
A 30-as pinen lévő fizikai gomb további vezérlést biztosít:

**Rövid Nyomás (< 3 másodperc):**
- Teszt módok közötti váltás: Rainbow → Red → Green → Blue → Rainbow

**Hosszú Nyomás (3 másodperc):**
- Váltás Art-Net mód és teszt mód között
- LED kétszer villan módváltáskor

**Hálózat Reset (10 másodperc):**
- Hálózati beállítások visszaállítása alapértelmezettre
- LED háromszor villan nyomás közben

**Teljes Reset (20 másodperc):**
- Minden beállítás visszaállítása gyári alapértelmezettre
- LED folyamatosan világít nyomás közben

## Használat

### Art-Net Beállítás
1. Konfigurálja világítástechnikai szoftverét (MadMapper, Resolume, stb.)
2. Állítsa be az Art-Net kimenetet a vezérlő IP címére
3. Konfigurálja az univerzumokat a kimenet leképezés szerint
4. Minden 1020 pixeles kimenet 6 univerzumot igényel (510 csatorna/univerzum)

### Univerzum Számítás
**Képlet:**
- Vég Univerzum = Start Univerzum + ((Pixelek × 3 + Start Address - 2) ÷ 510)
- Vég Address = ((Pixelek × 3 + Start Address - 1) % 510) + 1

**Példa (1020 pixel, 0 Univerzum, 1 Address):**
- Szükséges csatornák: 1020 × 3 = 3060
- Használt univerzumok: 0, 1, 2, 3, 4, 5 (összesen 6)
- Vég address az 5. univerzumban: 510

### Státusz Jelzők

**Beépített LED (13-as pin):**
- **Art-Net Mód:** Lassú villogás (1 Hz)
- **Teszt Mód:** Gyors villogás (4 Hz)
- **Gomb Nyomás:** Speciális minták hosszú nyomások alatt
- **Hálózati Aktivitás:** Rövid felvillanások adatfogadáskor

**Web Felület Státusz:**
- **Teszt Mód Aktív:** Zöld üzenet megjelenítése
- **Konfiguráció Változások:** Automatikus újraindítás értesítés
- **Kapcsolat Státusz:** Valós idejű frissítések

## Hibaelhárítás

### Hálózati Problémák

**Nem Elérhető a Web Felület:**
1. Ellenőrizze az Ethernet kábel csatlakozását
2. Győződjön meg róla, hogy a számítógép és vezérlő ugyanazon a hálózaton van
3. Próbálja ki a ping tesztet: `ping 10.10.0.42`
4. Ellenőrizze IP ütközéseket a hálózaton
5. Ha több hálózati interfész aktív, kapcsolja ki ideiglenesen a Wi-Fi-t

**Art-Net Nem Működik:**
1. Ellenőrizze, hogy a világítástechnikai szoftver Art-Net kimenete engedélyezett
2. Ellenőrizze az univerzum leképezés egyezését a vezérlő konfigurációjával
3. Erősítse meg az IP címet a világítástechnikai szoftverben
4. Tesztelje először egyszerű mintákkal
5. Figyelje a beépített LED aktivitását

### LED Problémák

**Nincs LED Kimenet:**
1. Ellenőrizze a LED szalagok tápellátását
2. Ellenőrizze a helyes pin csatlakozásokat
3. Tesztelje először a beépített teszt módokkal
4. Ellenőrizze a LED típus és szín sorrend beállításokat
5. Győződjön meg róla, hogy az adatvonal csatlakozások biztonságosak

**Helytelen Színek:**
1. Állítsa be a szín sorrend beállítását (RGB, GRB, stb.)
2. Ellenőrizze, hogy a LED típus megegyezik a konfigurációval
3. Ellenőrizze a feszültségesést hosszú LED futásoknál
4. Tesztelje egyszínű módokkal

**Villogás vagy Hibák:**
1. Ellenőrizze a tápegység kapacitását
2. Adjon hozzá leválasztó kondenzátorokat a LED-ek közelében
3. Csökkentse a fényerőt az áramfelvétel csökkentése érdekében
4. Ellenőrizze az adatvonal jel integritását
5. Ellenőrizze az elektromágneses interferenciát

### Univerzum Ütközések

**Átfedő Univerzumok:**
- Minden kimenetnek egyedi univerzum tartományt kell használnia
- Alapértelmezett 8 kimenetes távolság: 0, 6, 12, 18, 24, 30, 36, 42
- Állítsa be a start univerzumokat az ütközések elkerülése érdekében

**Hiányzó Adatok:**
1. Ellenőrizze az univerzum számozást a világítástechnikai szoftverben
2. Ellenőrizze a start univerzum és address beállításokat
3. Győződjön meg róla, hogy a világítástechnikai szoftver minden szükséges univerzumot küld
4. Figyelje a hálózati forgalmat olyan eszközökkel, mint a Wireshark

### Memória Problémák

**Rendszer Automatikusan Újraindul:**
1. A teljes pixel szám meghaladhatja a memória korlátokat
2. Csökkentse a pixeleket kimenetenként
3. Használjon kevesebb aktív kimenetet
4. Ellenőrizze a tápegység stabilitását

**Konfiguráció Nem Mentődik:**
1. Engedje meg a teljes újraindítási ciklust (várjon 10 másodpercet)
2. Ellenőrizze az EEPROM memória integritását
3. Állítsa vissza gyári alapértelmezettre, ha sérült
4. Kerülje a tápellátás megszakítását mentés közben

### Teljesítmény Problémák

**Lassú Válasz:**
1. Ellenőrizze a hálózati sávszélesség használatát
2. Csökkentse az Art-Net képkocka sebességet a szoftverben
3. Optimalizálja a világítástechnikai szoftver beállításait
4. Figyelje a csomagvesztést

**Egyenetlen LED Frissítések:**
1. Ellenőrizze, hogy minden szükséges univerzum küldésre kerül
2. Ellenőrizze a hálózati időzítési problémákat
3. Biztosítsa a konzisztens képkocka sebességet a szoftverből
4. Tesztelje csökkentett pixel számokkal

## Gyári Reset

### 1. Módszer: Gomb Reset
1. Tartsa nyomva a gombot 20 másodpercig
2. A LED folyamatos fényt mutat
3. Engedje el, amikor a LED kialszik
4. A rendszer alapértelmezett beállításokkal indul újra

### 2. Módszer: Web Felület
1. Nyissa meg a web felületet
2. Állítsa be minden értéket manuálisan alapértelmezettre
3. Mentse a konfigurációt
4. A rendszer újraindul

### Alapértelmezett Beállítások Reset Után
- **IP:** 10.10.0.42
- **Alhálózat:** 255.255.255.0
- **MAC:** 04:E5:E9:00:69:EC
- **Minden Kimenet:** 200 pixel
- **LED Típus:** WS2812/WS2812B
- **Szín Sorrend:** GRB
- **Fényerő:** 255 (100%)
- **Univerzumok:** 0, 1, 2, 3, 4, 5, 6, 7

## Műszaki Támogatás

### Diagnosztikai Információk
Problémák jelentésekor kérjük, mellékelje:
1. Vezérlő verzió (8/16/32 kimenet)
2. Hálózati konfiguráció
3. LED típusok és mennyiségek
4. Világítástechnikai szoftver és verzió
5. Hálózati topológia
6. Hiba tünetek és időzítés

### Gyakori Megoldások
- **Web felület timeout:** Frissítse a böngészőt, ellenőrizze a hálózatot
- **Részleges LED működés:** Ellenőrizze az univerzum leképezést
- **Rendszer instabilitás:** Ellenőrizze a tápegység kapacitását
- **Hálózati ütközések:** Használjon hálózati szkennert az IP használat ellenőrzésére

### Speciális Konfiguráció
Összetett beállításokhoz, amelyek egyedi univerzum leképezést vagy speciális LED konfigurációkat igényelnek, tekintse meg a Fejlesztői Dokumentációt részletes műszaki információkért.



