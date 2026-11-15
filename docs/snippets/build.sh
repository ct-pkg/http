ct build main.ct -I . -o app --outdir build \
    --cflags "-Wall -Wextra -Werror" --ldflags "-lcurl"
