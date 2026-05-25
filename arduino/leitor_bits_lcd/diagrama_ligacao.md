# Diagrama de Ligação – Leitor de Bits com LCD I2C

## Componentes

- Arduino Uno / Nano
- Display LCD 16x2 com módulo I2C (PCF8574)
- Fonte de bits (outro Arduino, sensor, shift register, etc.)

## LCD I2C → Arduino

| LCD I2C | Arduino Uno/Nano |
|---------|-----------------|
| VCC     | 5V              |
| GND     | GND             |
| SDA     | A4              |
| SCL     | A5              |

> No **Arduino Mega**: SDA → pino 20, SCL → pino 21.

## Fonte de Bits → Arduino

| Sinal | Arduino | Descrição                              |
|-------|---------|----------------------------------------|
| DATA  | Pino 4  | Nível lógico do bit atual (0 ou 1)    |
| CLOCK | Pino 2  | Pulso de subida = lê o bit do DATA    |
| GND   | GND     | Referência comum obrigatória           |

## Protocolo de Transmissão

```
CLOCK  ___     ___     ___     ___     ___     ___     ___     ___
      |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
  ____|   |___|   |___|   |___|   |___|   |___|   |___|   |___|   |___

DATA   [b7]    [b6]    [b5]    [b4]    [b3]    [b2]    [b1]    [b0]
        ^       ^       ^       ^       ^       ^       ^       ^
        |       |       |       |       |       |       |       |
        +-------+-------+-------+-------+-------+-------+-------+
                    Cada borda de subida do CLOCK lê 1 bit
                    MSB (b7) enviado primeiro
```

Após 8 pulsos de CLOCK, o byte completo é exibido no LCD:

```
+----------------+
| Dec: 173       |   ← valor decimal (0–255)
| Bin: 10101101  |   ← representação binária
+----------------+
```

## Biblioteca Necessária

Instale via **Arduino IDE → Gerenciar Bibliotecas**:

- **LiquidCrystal I2C** by Frank de Brabander

## Endereço I2C do LCD

O endereço padrão é `0x27`. Se o display não inicializar, execute o
sketch `I2C_Scanner` para descobrir o endereço correto e ajuste a
linha no código:

```cpp
LiquidCrystal_I2C lcd(0x27, 16, 2);  // troque 0x27 se necessário
```
