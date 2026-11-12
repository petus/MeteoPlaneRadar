# Karty

Karta je malý soubor JSON, který přidá na displej další obrazovku s několika
hodnotami staženými z internetu nebo z domácí sítě: kurzy měn, teplotu z Home
Assistantu, cenu čehokoli, co má veřejné JSON rozhraní.

Karta je **popis, ne program**. Říká, odkud se má stáhnout odpověď, které
hodnoty z ní vybrat a jak je pojmenovat. Rozvržení na kulatém displeji si
firmware dělá sám.

## Jak kartu přidat

1. Na webu zařízení, záložka **Obrazovky**, zapněte obrazovku **Karty** a uložte.
2. Ve stejné záložce v části **Karty** zvolte **Přidat kartu ze souboru**
   a vyberte soubor `.json`.
3. Je-li nastavené heslo správce, vyplňte ho předtím na záložce **Systém**.

Karet může být nejvýš 8. **Každá karta je samostatná obrazovka:** střídá se
s ostatními ve stejném intervalu a přechází se na ni stejně jako na kteroukoli
jinou. Se čtyřmi vestavěnými obrazovkami a dvěma kartami se jich tedy střídá
šest. Karta se stejným `id` nahradí tu původní.

Řada teček nahoře, která má jednu tečku za každou obrazovku i kartu, je ve
výchozím stavu skrytá, protože s několika kartami je dlouhá a málo říká.
Zapíná se na záložce **Obrazovky** volbou **Zobrazovat tečky obrazovek
nahoře**.

## Ukázka: kurzy ČNB

[`kurzy-cnb.json`](kurzy-cnb.json) — kurz eura a dolaru podle kurzovního lístku
České národní banky.

```json
{
  "v": 1,
  "id": "kurzy-cnb",
  "title": "Kurzy ČNB",
  "url": "https://api.cnb.cz/cnbapi/exrates/daily?lang=EN",
  "refresh_min": 60,
  "rows": [
    { "label": "1 EUR", "path": "rates[currencyCode=EUR].rate", "decimals": 3, "unit": "Kč" },
    { "label": "1 USD", "path": "rates[currencyCode=USD].rate", "decimals": 3, "unit": "Kč" }
  ],
  "note": { "label": "ČNB", "path": "rates[0].validFor" }
}
```

## Ukázka: svátek

[`svatek.json`](svatek.json) — kdo má dnes svátek, podle
[nameday.abalin.net](https://nameday.abalin.net). Hodnota nemusí být číslo:
tady je to text a cesta k němu je obyčejná cesta objektem. Parametr `timezone`
v adrese říká zdroji, podle kterého času má počítat „dnes“.

```json
{
  "v": 1,
  "id": "svatek",
  "title": "Svátek",
  "url": "https://nameday.abalin.net/api/V2/today?timezone=Europe/Prague",
  "refresh_min": 60,
  "rows": [
    { "label": "dnes má svátek", "path": "data.cz" }
  ]
}
```

O některých svátcích zdroj ke jménu přidává i název svátku („Adam a Eva, Štědrý
den“) a takový text se na displeji ořízne — viz délka hodnoty níže.

## Formát

| Pole | Povinné | Význam |
| --- | --- | --- |
| `v` | ne | verze formátu, zatím vždy `1` |
| `id` | ano | 1–23 znaků z `a-z`, `0-9`, `-`; je to i jméno souboru v zařízení |
| `title` | ano | nadpis karty, nejvýš 27 bajtů |
| `url` | ano | adresa začínající `http://` nebo `https://`, která vrací JSON; nejvýš 199 znaků |
| `refresh_min` | ne | jak často stahovat, v minutách; 5 až 1440, výchozí 60 |
| `rows` | ano | 1 až 4 řádky, viz níže |
| `note` | ne | malý řádek dole: `path` a volitelně `label` |

Řádek (`rows[]`):

| Pole | Povinné | Význam |
| --- | --- | --- |
| `path` | ano | kde v odpovědi hodnota je, viz níže; nejvýš 71 znaků |
| `label` | ne | popisek nad hodnotou, nejvýš 19 bajtů |
| `unit` | ne | jednotka za hodnotou, nejvýš 9 bajtů |
| `decimals` | ne | počet desetinných míst, 0 až 6. Bez něj: celá čísla beze změny, ostatní na 2 místa |
| `scale` | ne | čím hodnotu vynásobit před zobrazením (např. `0.001` z Wh na kWh) |

Délky jsou v bajtech UTF-8: písmeno s háčkem nebo čárkou zabere dva.
Text, který se do pole nevejde, zařízení odmítne a napíše, které pole to bylo.

### Cesta k hodnotě

| Zápis | Význam |
| --- | --- |
| `data.amount` | prvek objektu |
| `list[0].price` | prvek pole podle pořadí (od nuly) |
| `rates[currencyCode=EUR].rate` | první prvek pole, jehož `currencyCode` je `EUR` |

Třetí tvar použijte všude, kde pořadí v poli není zaručené. Kurzovní lístek je
takový případ: `rates[7]` by po přidání jedné měny ukázalo jinou.

Hodnota může být číslo, text nebo `true`/`false`. Text, který je ve skutečnosti
číslo (`"97123.50"`), se jako číslo zformátuje, pokud řádek uvádí `decimals`
nebo `scale`. V češtině se desetinná tečka čísel zobrazí jako čárka.

Zobrazená hodnota má nejvýš 23 bajtů (malý řádek dole 27); delší text se
ořízne na hranici znaku.

## Co je dobré vědět

- **Karta nedělá nic jiného, než že stahuje adresu a ukazuje hodnoty.** Přesto
  přidávejte jen karty, kterým rozumíte: adresu, na kterou se bude zařízení
  pravidelně ptát, určuje autor karty.
- Odpověď smí mít nejvýš tolik, kolik firmware dovolí pro textové odpovědi
  (`NET_MAX_TEXT` v `Config.h`). Do paměti se z ní ukládají jen hodnoty, které
  karta čte.
- HTTP hlavičky (např. `Authorization` pro Home Assistant) karta zatím poslat
  neumí. Funguje tedy jen to, co jde přečíst bez přihlášení.
- Když se stažení nepovede, zůstanou na displeji poslední hodnoty s poznámkou
  o starších datech a zařízení to za dvě minuty zkusí znovu. Stav posledního
  stažení je na webu na záložce Ovládání v tabulce stavu.
- S pixelovým písmem se diakritika v textech karty zobrazí bez háčků a čárek
  („Kč“ jako „Kc“). Vyhlazená písma ji umí; znak, který písmo nemá (např. €),
  se vynechá — pište raději `EUR`.
- Dokud je obrazovka Karty vypnutá, zařízení kvůli kartám nic nestahuje.

## Přidání vlastní karty do repozitáře

Pošlete pull request se souborem `cards/<id>.json`. Karta by měla používat
zdroj, který je veřejný, bez klíče a jehož podmínky takové použití dovolují.
