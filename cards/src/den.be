# Kolik dne už uběhlo: prstenec a čas uprostřed. Nepotřebuje internet,
# jen hodiny zařízení; překresluje se sama podle "redraw_s" v kartě.
def draw()
  var s = cfg.hms()
  g.ring(240, 240, 150, 168, 0, 360, g.DKGRAY)
  if s == nil
    g.label(cfg.lang() == 'en' ? 'waiting for the clock' : 'čekám na čas', 232, 2, g.YELLOW)
    return
  end
  g.ring(240, 240, 150, 168, 0, s * 360 / 86400, g.CYAN)
  g.label(format('%02d:%02d', s / 3600, (s % 3600) / 60), 190, 5, g.WHITE)
  var txt = cfg.lang() == 'en' ? '%d %% of the day' : '%d %% dne'
  g.label(format(txt, s * 100 / 86400), 246, 2, g.GRAY)
  g.label(cfg.title(), 286, 2, g.CYAN)
end
