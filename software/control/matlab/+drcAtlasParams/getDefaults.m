function param_sets = getDefaults(r)
typecheck(r, 'DRCAtlas');
param_sets = struct('standing_hardware', drcAtlasParams.StandingHardware(r),...
                    'standing_sim', drcAtlasParams.StandingSim(r),...
                    'walking_hardware', drcAtlasParams.WalkingHardware(r),...
                    'walking_sim', drcAtlasParams.WalkingSim(r),...
                    'manip_hardware', drcAtlasParams.ManipHardware(r),...
                    'recovery_hardware', drcAtlasParams.RecoveryHardware(r),...
                    'recovery_sim', drcAtlasParams.RecoverySim(r));
                    'bracing_sim', drcAtlasParams.BracingSim(r),...
                    'bracing_hardware', drcAtlasParams.BracingHardware(r),...
                    'manip_sim', drcAtlasParams.ManipSim(r));
  