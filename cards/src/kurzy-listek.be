# Kurzovní lístek ČNB: více měn pod sebou, částka vlevo, kurz zarovnaný vpravo.
# Které měny se ukážou, říká "params.codes" v kartě.
import string

def draw()
  g.label(cfg.title(), 74, 3, g.CYAN)
  var d = net.data()
  if d == nil
    g.label(cfg.lang() == 'en' ? 'loading' : 'načítám', 228, 2, g.YELLOW)
    return
  end
  var by = {}
  for r : d['rates'] by[r['currencyCode']] = r end
  var y = 126
  for code : cfg.get('codes', ['EUR', 'USD'])
    var r = by.find(code)
    if r != nil && y < 340
      var rate = format('%.3f', r['rate'])
      if cfg.lang() != 'en' rate = string.replace(rate, '.', ',') end
      g.text(format('%d %s', r['amount'], code), 112, y, 3, g.GRAY)
      g.text(rate, 368 - g.text_w(rate, 3), y, 3, g.WHITE)
      y += 38
    end
  end
  g.label('ČNB ' + d['rates'][0]['validFor'], 404, 2, g.GRAY)
end
