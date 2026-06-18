# Horloge GPS avec reveils - STM32F746NG-DISCO

Projet realise sur carte **STM32F746NG-DISCO** avec PlatformIO, LVGL et FreeRTOS.

La branche a consulter pour la version finale du projet est :

```text
feature/alarm-interface
```

## Objectif

Le projet consiste a realiser une horloge synchronisee par GPS, avec une interface graphique tactile et un systeme de reveils sonores. Une fois l'heure GPS valide recue, la carte continue de faire avancer l'horloge localement a l'aide des ticks FreeRTOS, meme si le signal GPS est ensuite perdu.

## Materiel utilise

- Carte STM32F746NG-DISCO
- Module GPS communiquant en UART
- Buzzer commande en PWM
- Ecran tactile integre a la carte DISCO

## Raccordements principaux

| Element | Broche composant | Broche DISCO-F746NG | Role |
| --- | --- | --- | --- |
| GPS | VCC | 3.3 V | Alimentation |
| GPS | GND | GND | Masse commune |
| GPS | TX | PF6 / RX7 | Reception UART cote carte |
| GPS | RX | PF7 / TX7 | Emission UART cote carte |
| Buzzer | Signal | PC7 / PWM8-2 | Commande PWM |
| Buzzer | VCC | 3.3 V | Alimentation |
| Buzzer | GND | GND | Masse commune |

Le schema de raccordement a ete realise avec KiCad.

## Fonctionnalites

- Lecture des trames GPS NMEA sur UART7 a 9600 bauds.
- Synchronisation uniquement lorsqu'une trame GPS valide avec fix est recue.
- Conversion de l'heure UTC en heure locale francaise avec un decalage de +2 h.
- Affichage d'une horloge numerique et d'une horloge analogique.
- Paysage anime qui change selon l'heure :
  - jour de 09 h a 21 h avec soleil, mer et nuages ;
  - nuit avec lune et etoiles.
- Menu de configuration des reveils avec transition sous la mer.
- Selection de l'heure et des minutes via rollers LVGL.
- Gestion de plusieurs reveils sans doublon d'horaire.
- Declenchement du buzzer par melodie PWM.
- Actions disponibles lors d'un reveil : Stop ou report de 5 minutes.

## Architecture logicielle

Le fichier principal est :

```text
src/main.cpp
```

L'interface graphique est construite avec **LVGL**. La gestion periodique du GPS, du compteur local et du buzzer est effectuee dans une tache FreeRTOS appelee depuis le template `lvglDrivers`.

Les principales parties du code sont :

- creation de l'interface graphique ;
- lecture et validation des trames GPS ;
- maintien local de l'heure apres synchronisation ;
- gestion des reveils ;
- pilotage du buzzer en PWM.

## Compilation

Depuis PlatformIO.

