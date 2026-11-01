"""Preserve reviewed legacy adapter authority without adding controller acquisitions."""
import copy

# Exact original grants reported by both published product inspections.
# These are optional adapter declarations, not new provider bindings.
LEGACY_GRANTS=(
    {'capability':'bluetooth.hci','api':1,'instance_id':16},
    {'capability':'app.catalog','api':1,'instance_id':0},
)

def project_authority(canonical, grants, retired):
    selected=copy.deepcopy(canonical)
    retained=[copy.deepcopy(g) for g in grants if g['capability'] not in retired]
    radio=[g for g in retained if g['capability']=='radio.lora']
    if radio:
        # Input identity is established by the assembler's pinned whole-artifact
        # SHA256 before decoding. Instance0 in an app request means unique in
        # Runtime; it does not constrain an original product's provider ID.
        if (len(radio)!=1 or set(radio[0])!={'capability','api','instance_id'}
                or type(radio[0]['api']) is not int or radio[0]['api']!=2
                or type(radio[0]['instance_id']) is not int
                or not 0<=radio[0]['instance_id']<=2147483647):
            raise ValueError('Malformed or conflicting original LoRa radio authority')
    # Absence is intentional: the controller exposes unavailable radio without
    # inventing a provider or grant for products that never had one.
    added=[]
    for expected in LEGACY_GRANTS:
        matches=[g for g in retained if g['capability']==expected['capability']]
        if not matches:continue
        if len(matches)!=1 or matches[0]!=expected or any(type(matches[0][k]) is not int for k in ('api','instance_id')):
            raise ValueError('Unreviewed legacy LoRa grant: '+expected['capability'])
        declaration={'capability':expected['capability'],'api':expected['api']}
        existing=[d for d in selected['optional']+selected['requires'] if d['capability']==expected['capability']]
        if existing and existing!=[declaration]:raise ValueError('Conflicting legacy declaration')
        if not existing:selected['optional'].append(declaration);added.append(declaration)
    declared={(d['capability'],d['api']) for d in selected['optional']+selected['requires']}
    if not all((g['capability'],g['api']) in declared for g in retained):
        raise ValueError('Unreviewed original LoRa authority; do not discard it')
    return selected,retained,added
