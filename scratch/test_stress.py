from espyak.api import G2P
g = G2P('de')
g._tr.dict.words = {}
print(g.phonemize('"Kinder%stube', ipa=False))
print(g.phonemize("'Kinder,stube", ipa=False))
print(g.phonemize('Kinder-stube', ipa=False))
