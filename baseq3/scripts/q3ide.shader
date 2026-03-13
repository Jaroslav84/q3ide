/* Black background + dim logo: shown on windows that have no streaming texture yet.
 * Lets you spot freshly placed windows that aren't capturing. */
q3ide/bg
{
    cull disable
    nomipmaps
    nopicmip
    {
        map $whiteimage
        rgbGen const ( 0 0 0 )
        depthWrite
    }
    {
        map textures/sfx/logo512.jpg
        blendFunc add
        rgbGen const ( 0.25 0.25 0.25 )
    }
}

q3ide/mirror
{
    portal
    {
        map $whiteimage
        blendFunc GL_ZERO GL_ONE
        depthWrite
    }
}

q3ide/win0
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch0
        depthWrite
        rgbGen vertex
    }
}

q3ide/win1
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch1
        depthWrite
        rgbGen vertex
    }
}

q3ide/win2
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch2
        depthWrite
        rgbGen vertex
    }
}

q3ide/win3
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch3
        depthWrite
        rgbGen vertex
    }
}

q3ide/win4
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch4
        depthWrite
        rgbGen vertex
    }
}

q3ide/win5
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch5
        depthWrite
        rgbGen vertex
    }
}

q3ide/win6
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch6
        depthWrite
        rgbGen vertex
    }
}

q3ide/win7
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch7
        depthWrite
        rgbGen vertex
    }
}

q3ide/win8
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch8
        depthWrite
        rgbGen vertex
    }
}

q3ide/win9
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch9
        depthWrite
        rgbGen vertex
    }
}

q3ide/win10
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch10
        depthWrite
        rgbGen vertex
    }
}

q3ide/win11
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch11
        depthWrite
        rgbGen vertex
    }
}

q3ide/win12
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch12
        depthWrite
        rgbGen vertex
    }
}

q3ide/win13
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch13
        depthWrite
        rgbGen vertex
    }
}

q3ide/win14
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch14
        depthWrite
        rgbGen vertex
    }
}

q3ide/win15
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch15
        depthWrite
        rgbGen vertex
    }
}

q3ide/win16
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch16
        depthWrite
        rgbGen vertex
    }
}
q3ide/win17
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch17
        depthWrite
        rgbGen vertex
    }
}
q3ide/win18
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch18
        depthWrite
        rgbGen vertex
    }
}
q3ide/win19
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch19
        depthWrite
        rgbGen vertex
    }
}
q3ide/win20
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch20
        depthWrite
        rgbGen vertex
    }
}
q3ide/win21
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch21
        depthWrite
        rgbGen vertex
    }
}
q3ide/win22
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch22
        depthWrite
        rgbGen vertex
    }
}
q3ide/win23
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch23
        depthWrite
        rgbGen vertex
    }
}
q3ide/win24
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch24
        depthWrite
        rgbGen vertex
    }
}
q3ide/win25
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch25
        depthWrite
        rgbGen vertex
    }
}
q3ide/win26
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch26
        depthWrite
        rgbGen vertex
    }
}
q3ide/win27
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch27
        depthWrite
        rgbGen vertex
    }
}
q3ide/win28
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch28
        depthWrite
        rgbGen vertex
    }
}
q3ide/win29
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch29
        depthWrite
        rgbGen vertex
    }
}
q3ide/win30
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch30
        depthWrite
        rgbGen vertex
    }
}
q3ide/win31
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch31
        depthWrite
        rgbGen vertex
    }
}
q3ide/win32
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch32
        depthWrite
        rgbGen vertex
    }
}
q3ide/win33
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch33
        depthWrite
        rgbGen vertex
    }
}
q3ide/win34
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch34
        depthWrite
        rgbGen vertex
    }
}
q3ide/win35
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch35
        depthWrite
        rgbGen vertex
    }
}
q3ide/win36
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch36
        depthWrite
        rgbGen vertex
    }
}
q3ide/win37
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch37
        depthWrite
        rgbGen vertex
    }
}
q3ide/win38
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch38
        depthWrite
        rgbGen vertex
    }
}
q3ide/win39
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch39
        depthWrite
        rgbGen vertex
    }
}
q3ide/win40
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch40
        depthWrite
        rgbGen vertex
    }
}
q3ide/win41
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch41
        depthWrite
        rgbGen vertex
    }
}
q3ide/win42
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch42
        depthWrite
        rgbGen vertex
    }
}
q3ide/win43
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch43
        depthWrite
        rgbGen vertex
    }
}
q3ide/win44
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch44
        depthWrite
        rgbGen vertex
    }
}
q3ide/win45
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch45
        depthWrite
        rgbGen vertex
    }
}
q3ide/win46
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch46
        depthWrite
        rgbGen vertex
    }
}
q3ide/win47
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch47
        depthWrite
        rgbGen vertex
    }
}
q3ide/win48
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch48
        depthWrite
        rgbGen vertex
    }
}
q3ide/win49
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch49
        depthWrite
        rgbGen vertex
    }
}
q3ide/win50
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch50
        depthWrite
        rgbGen vertex
    }
}
q3ide/win51
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch51
        depthWrite
        rgbGen vertex
    }
}
q3ide/win52
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch52
        depthWrite
        rgbGen vertex
    }
}
q3ide/win53
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch53
        depthWrite
        rgbGen vertex
    }
}
q3ide/win54
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch54
        depthWrite
        rgbGen vertex
    }
}
q3ide/win55
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch55
        depthWrite
        rgbGen vertex
    }
}
q3ide/win56
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch56
        depthWrite
        rgbGen vertex
    }
}
q3ide/win57
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch57
        depthWrite
        rgbGen vertex
    }
}
q3ide/win58
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch58
        depthWrite
        rgbGen vertex
    }
}
q3ide/win59
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch59
        depthWrite
        rgbGen vertex
    }
}
q3ide/win60
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch60
        depthWrite
        rgbGen vertex
    }
}
q3ide/win61
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch61
        depthWrite
        rgbGen vertex
    }
}
q3ide/win62
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch62
        depthWrite
        rgbGen vertex
    }
}
q3ide/win63
{
    cull disable
    nomipmaps
    nopicmip
    {
        map *scratch63
        depthWrite
        rgbGen vertex
    }
}
