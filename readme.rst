Description
===========

MatchHistogram modifies one clip's histogram to match the histogram
of another clip.

Will produce weird results if frame contents are dissimilar.

Should be used for analysis only, not for production.

This is a port of the Avisynth plugin MatchHistogram from here:
https://forum.doom9.org/showthread.php?p=1729864#post1729864


Usage
=====
::

    matchhist.MatchHistogram(clip clip1, clip clip2, [clip clip3=clip1, bint raw=False, bint show=False, bint debug=False, int smoothing_window=8, int[] planes=0])


Parameters:
    *clip1*
        ???

        Must have constant format and dimensions and 8 bits per
        sample, and it must not be RGB.

    *clip2*
        Clip whose histogram is to be copied.

        Must have the same format and dimensions as *clip1*.

    *clip3*
        Clip to be modified to match *clip2*'s histogram.

        Must have the same format and dimensions as *clip1*.

        If this parameter is not passed then *clip1* is used instead.

        Default: *clip1*.

    *raw*
        Use the raw histogram without postprocessing.

        Default: False.

    *show*
        Show calculated curve on video frame.

        This parameter has no effect when *debug* is True.

        Default: False.

    *debug*
        Return 256x256 clip with calculated data.

        Default: False.

    *smoothing_window*
        Window used when smoothing the curve.

        A value of 0 disables the smoothing.

        This parameter has no effect when *raw* is True.

        Default: 8.

    *planes*
        Select which planes to process. Any unprocessed planes will be
        copied from the third clip.

        Default: 0 (the first plane only).


Compilation
===========

::

    meson build && cd build
    ninja


License
=======

???
