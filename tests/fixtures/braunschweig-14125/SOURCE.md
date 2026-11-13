# TU Braunschweig ISO 14125 four-point bending

The `.CSV` here is one raw testXpert III export (`Hemp_reference`, specimen
Hanf80°Ctrocken_Ref_32) from:

> Osten, A., Steinmetz, J., Charif, S., Voelkerink, O. (2025). *Moisture
> uptake, flexural properties, and mode I fracture toughness of hemp fiber
> reinforced polymer laminates with embedded capacitive sensor.* Zenodo.
> https://doi.org/10.5281/zenodo.17611621

Licence: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) (read
on the Zenodo record and in the archive's README, 2026-11-13). The file is
unchanged, copied from `Four-Point Bending Tests.zip` (SHA-256
f1837dc5027b41937e2bb5721a55480234a1a7e09a02cb34ea8703f85cf300c8).

It is committed as the importer's and the window's example of testXpert's
dialect (Latin-1, a "key   value unit" header, no units row, decimal
commas). The other 24 exports are fetched at test time from the same
archive by the validation (`braunschweig_14125_agrees_with_testxpert`);
testXpert's results for all 25 are transcribed in
`tests/validation/braunschweig-14125-agreed.json`.
