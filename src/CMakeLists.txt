add_mandrill_executable(DSRT)

target_sources(DSRT
	PRIVATE
		"DSRT.cpp"
)

add_shaders(DSRT
	"GBuffer.vert"
	"GBuffer.frag"
	"Resolve.vert"
	"Resolve.frag"
	"RayGen.rgen"
	"RayMiss.rmiss"
	"RayClosestHit.rchit"
)
