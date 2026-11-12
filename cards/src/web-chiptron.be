# Žije web? Zařízení každých 5 minut stáhne malou odpověď webu (adresa
# v kartě). Povede se = zelený kruh s fajfkou. Nepovede se = zařízení to
# za 2 minuty zkusí znovu; do třetího nezdaru za sebou je stav oranžový
# ("ověřuji"), po třetím červený ("web nežije").
import math

# 3 kontroly: ok -> +5 min nezdar -> +2 min nezdar -> +2 min nezdar.
# Třetí nezdar tedy přijde asi 9 minut po poslední dobré odpovědi.
var DOWN_AFTER_S = 540

# Tlustá čára z kruhů - fajfka, křížek, vykřičník.
def stroke(x0, y0, x1, y1, r, col)
  var n = int(math.sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)) / 3) + 1
  for i : 0 .. n
    g.fill_circle(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, r, col)
  end
end

def ago(s)
  var en = cfg.lang() == 'en'
  if s < 90 return format(en ? '%d s ago' : 'před %d s', s) end
  if s < 5400 return format(en ? '%d min ago' : 'před %d min', (s + 30) / 60) end
  return format(en ? '%d h ago' : 'před %d h', s / 3600)
end

def badge(cx, cy, fill, rim)
  g.ring(cx, cy, 86, 93, 0, 360, rim)
  g.fill_circle(cx, cy, 78, fill)
end

def draw()
  var en = cfg.lang() == 'en'
  var cx = 240
  var cy = 222
  g.label(cfg.title(), 84, 3, g.CYAN)
  var age = net.age()

  if net.ok()
    var green = g.rgb(34, 197, 94)
    badge(cx, cy, green, g.rgb(20, 83, 45))
    stroke(cx - 42, cy + 2, cx - 12, cy + 32, 10, g.WHITE)
    stroke(cx - 12, cy + 32, cx + 46, cy - 30, 10, g.WHITE)
    g.label(en ? 'site is up' : 'web žije', 336, 3, green)
    g.label((en ? 'checked ' : 'ověřeno ') + ago(age), 378, 2, g.GRAY)
  elif age == nil
    # nic se ještě nepovedlo: buď kontrola teprve proběhne, nebo web
    # neodpověděl ani jednou od zapnutí
    g.ring(cx, cy, 86, 93, 0, 360, g.DKGRAY)
    g.label(en ? 'waiting for an answer' : 'čekám na odpověď', 336, 2, g.GRAY)
  elif age < DOWN_AFTER_S
    var amber = g.rgb(245, 158, 11)
    badge(cx, cy, amber, g.rgb(120, 53, 15))
    stroke(cx, cy - 46, cx, cy + 10, 10, g.WHITE)
    g.fill_circle(cx, cy + 40, 11, g.WHITE)
    g.label(en ? 'no answer, checking' : 'neodpovídá, ověřuji', 336, 3, amber)
    g.label((en ? 'last up ' : 'naposledy OK ') + ago(age), 378, 2, g.GRAY)
  else
    var red = g.rgb(239, 68, 68)
    badge(cx, cy, red, g.rgb(127, 29, 29))
    stroke(cx - 32, cy - 32, cx + 32, cy + 32, 10, g.WHITE)
    stroke(cx + 32, cy - 32, cx - 32, cy + 32, 10, g.WHITE)
    g.label(en ? 'site is down' : 'web nežije', 336, 3, red)
    g.label((en ? 'last up ' : 'naposledy OK ') + ago(age), 378, 2, g.GRAY)
  end
end
