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

## Obchod s kartami

Karty z této složky jde přidat i bez stahování souboru: na webu zařízení,
záložka **Obchod**, tlačítko **Načíst nabídku** a u vybrané karty **Přidat**.

Jak to funguje:

1. Zařízení si samo stáhne seznam [`index.json`](index.json) a jeho podpis
   [`index.json.sig`](index.json.sig). Adresu má ve firmwaru
   (`STORE_INDEX_URL` v `Config.h`).
2. Podpis ověří proti veřejnému klíči ve firmwaru (`StoreKey.h`). Seznam,
   jehož podpis nesouhlasí, nepřijme a nic z něj nenabídne.
3. Při přidání stáhne soubor karty a porovná ho s otiskem SHA-256, který pro
   něj uvádí seznam. Teprve pak projde karta stejnou kontrolou jako karta
   nahraná ručně.

K čemu podpis je: firmware při HTTPS neověřuje certifikáty, takže bez podpisu
by nabídku mohl podvrhnout kdokoli na cestě. K čemu není: neříká, že kartu
někdo prověřil — jen že seznam zveřejnil držitel klíče. Karta je pořád jen
popis, žádný program.

Seznam má pořadové číslo. Zařízení si pamatuje nejvyšší, které přijalo,
a starší seznam odmítne, takže stažená karta nejde vrátit zpět podstrčením
staré, správně podepsané nabídky.

Po změně nabídky může GitHub ještě pár minut vracet starší verzi některého
souboru. Zařízení pak nabídku nebo kartu odmítne; stačí to za chvíli zkusit
znovu.

### Správa nabídky

Seznam a podpis vyrábí `tools/make_index.py`:

```
python3 tools/make_index.py            # po přidání nebo změně karty
python3 tools/make_index.py --check    # jen ověří, že seznam a podpis sedí
```

Nástroj projde `cards/*.json`, zkontroluje je, spočítá otisky, zvýší pořadové
číslo a seznam podepíše soukromým klíčem. Ten se hledá v `--key`, v proměnné
`MPR_STORE_KEY` a v `~/.config/meteoplaneradar/store-signing-key.pem`; do
repozitáře nepatří. Popisky karet v nabídce jsou v
[`store-meta.json`](store-meta.json).

Vlastní obchod: vytvořte si pár klíčů
(`openssl ecparam -name prime256v1 -genkey -noout -out klic.pem`), veřejný
klíč (`openssl ec -in klic.pem -pubout`) dejte do `StoreKey.h`, v `Config.h`
změňte `STORE_INDEX_URL` na adresu svého seznamu a firmware přeložte.

## Přidání vlastní karty do repozitáře

Pošlete pull request se souborem `cards/<id>.json` a popiskem v
`cards/store-meta.json`. Seznam obchodu podepisuje správce repozitáře. Karta by měla používat
zdroj, který je veřejný, bez klíče a jehož podmínky takové použití dovolují.
