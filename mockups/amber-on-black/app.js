(() => {
  "use strict";

  const WIDTH = 240;
  const HEIGHT = 320;
  const screenCanvas = document.getElementById("device-screen");
  const screenContext = screenCanvas.getContext("2d", { willReadFrequently: true });
  const screenWindow = document.getElementById("screen-window");
  const screenOff = document.getElementById("screen-off");
  const screenLoading = document.getElementById("screen-loading");
  const preserveArtToggle = document.getElementById("preserve-art");
  const activeTitle = document.getElementById("active-screen-title");
  const activeIndex = document.getElementById("screen-index");
  const activeDescription = document.getElementById("screen-description");
  const playStateLabel = document.getElementById("play-state");
  const playStateDot = document.getElementById("play-dot");
  const volumeValueLabel = document.getElementById("volume-value");
  const volumeFill = document.getElementById("volume-fill");
  const toast = document.getElementById("toast");

  const screenDefinitions = [
    {
      id: "home",
      title: "Home",
      source: "../../screenshots/tokyonight/main-menu.png",
      description: "Six destinations, a compact status bar and the live mini-player strip.",
    },
    {
      id: "music",
      title: "Music",
      source: "../../screenshots/tokyonight/music.png",
      description: "Browse shortcuts above the compact music-library list.",
    },
    {
      id: "nowPlaying",
      title: "Now playing",
      source: "../../screenshots/now-playing.png",
      description: "Album art stays natural; the transport, progress and chrome turn amber.",
    },
    {
      id: "files",
      title: "File browser",
      source: "../../screenshots/file-browser.png",
      description: "The V1 file list and album thumbnails, recoloured around the original art.",
    },
    {
      id: "streaming",
      title: "Streaming",
      source: "../../screenshots/tokyonight/streaming.png",
      description: "Radio, podcasts and streaming services in the real compact list layout.",
    },
    {
      id: "wireless",
      title: "Wireless",
      source: "../../screenshots/tokyonight/wireless.png",
      description: "Wi-Fi, Bluetooth and the wireless action tiles, rendered from the V1 capture.",
    },
    {
      id: "more",
      title: "More",
      source: "../../screenshots/tokyonight/more.png",
      description: "DAC, file manager, Gearboy and Books in the V1's four-row More page.",
    },
    {
      id: "settings",
      title: "Settings",
      source: "../../screenshots/tokyonight/settings.png",
      description: "Appearance, Screen and Date and time—the familiar V1 settings list.",
    },
  ];

  const byId = new Map(screenDefinitions.map((screen, index) => [screen.id, { ...screen, index }]));
  const sourcePixels = new Map();
  const imageLoads = new Map();
  let currentScreen = "home";
  let history = [];
  let screenIsOn = true;
  let isPlaying = true;
  let volume = 61;
  let toastTimer;
  let loadedCount = 0;

  // A luma-to-amber transfer curve keeps antialiased text and icon edges smooth,
  // while moving the V1's blue-black UI into a restrained black / gold palette.
  const neutralStops = [
    [0,   [0, 0, 0]],
    [12,  [2, 2, 2]],
    [24,  [5, 5, 5]],
    [33,  [7, 7, 6]],
    [41,  [11, 10, 7]],
    [47,  [23, 18, 10]],
    [54,  [37, 26, 11]],
    [65,  [52, 34, 12]],
    [82,  [80, 49, 14]],
    [103, [119, 73, 20]],
    [128, [158, 97, 29]],
    [153, [194, 122, 39]],
    [180, [222, 155, 66]],
    [205, [244, 188, 93]],
    [226, [255, 211, 133]],
    [244, [255, 230, 176]],
    [255, [255, 242, 207]],
  ];

  const accentStops = [
    [0,   [0, 0, 0]],
    [16,  [3, 3, 2]],
    [28,  [6, 6, 4]],
    [40,  [12, 9, 5]],
    [50,  [53, 32, 7]],
    [60,  [110, 63, 9]],
    [72,  [174, 101, 15]],
    [88,  [213, 128, 24]],
    [108, [238, 147, 31]],
    [132, [255, 163, 35]],
    [160, [255, 180, 48]],
    [188, [255, 199, 77]],
    [216, [255, 219, 124]],
    [240, [255, 237, 183]],
    [255, [255, 246, 218]],
  ];

  function colorAtLuma(luma, stops) {
    if (luma <= stops[0][0]) return stops[0][1];
    for (let i = 1; i < stops.length; i += 1) {
      const [rightAt, rightColor] = stops[i];
      if (luma <= rightAt) {
        const [leftAt, leftColor] = stops[i - 1];
        const blend = (luma - leftAt) / (rightAt - leftAt);
        return [
          Math.round(leftColor[0] + (rightColor[0] - leftColor[0]) * blend),
          Math.round(leftColor[1] + (rightColor[1] - leftColor[1]) * blend),
          Math.round(leftColor[2] + (rightColor[2] - leftColor[2]) * blend),
        ];
      }
    }
    return stops[stops.length - 1][1];
  }

  function isAlbumArtwork(screenId, x, y) {
    if (!preserveArtToggle.checked) return false;

    // The capture's 156 px sleeve is retained in the same place it appears on
    // the actual player. It is artwork, not an interface colour.
    if (screenId === "nowPlaying") {
      return x >= 40 && x < 200 && y >= 17 && y < 177;
    }

    // The five small cover thumbnails in the repository's V1 file-browser
    // capture are treated as artwork too.
    if (screenId === "files" && x >= 8 && x < 48 && y >= 73 && y < 319) {
      const row = Math.floor((y - 73) / 48.5);
      const rowTop = 73 + row * 48.5;
      return y < rowTop + 48;
    }

    return false;
  }

  function recolorImage(screenId, original) {
    const data = new Uint8ClampedArray(original.data.length);
    for (let y = 0; y < HEIGHT; y += 1) {
      for (let x = 0; x < WIDTH; x += 1) {
        const i = (y * WIDTH + x) * 4;
        const r = original.data[i];
        const g = original.data[i + 1];
        const b = original.data[i + 2];
        const a = original.data[i + 3];

        if (isAlbumArtwork(screenId, x, y)) {
          data[i] = r;
          data[i + 1] = g;
          data[i + 2] = b;
          data[i + 3] = a;
          continue;
        }

        const luma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
        const chroma = Math.max(r, g, b) - Math.min(r, g, b);
        const tone = chroma > 58 && luma > 68
          ? colorAtLuma(luma, accentStops)
          : colorAtLuma(luma, neutralStops);

        data[i] = tone[0];
        data[i + 1] = tone[1];
        data[i + 2] = tone[2];
        data[i + 3] = a;
      }
    }
    return new ImageData(data, WIDTH, HEIGHT);
  }

  function prepareScreen(screen) {
    if (imageLoads.has(screen.id)) return imageLoads.get(screen.id);

    const promise = new Promise((resolve) => {
      const image = new Image();
      image.decoding = "async";
      image.onload = () => {
        const sourceCanvas = document.createElement("canvas");
        sourceCanvas.width = WIDTH;
        sourceCanvas.height = HEIGHT;
        const sourceContext = sourceCanvas.getContext("2d", { willReadFrequently: true });
        sourceContext.drawImage(image, 0, 0, WIDTH, HEIGHT);
        sourcePixels.set(screen.id, sourceContext.getImageData(0, 0, WIDTH, HEIGHT));
        loadedCount += 1;
        if (loadedCount === screenDefinitions.length) screenLoading.classList.add("is-hidden");
        if (screen.id === currentScreen) renderCurrentScreen();
        resolve(true);
      };
      image.onerror = () => {
        loadedCount += 1;
        if (loadedCount === screenDefinitions.length) screenLoading.classList.add("is-hidden");
        if (screen.id === currentScreen) showToast("Could not load a V1 capture. Check that the preview is served from the repository root.");
        resolve(false);
      };
      image.src = screen.source;
    });

    imageLoads.set(screen.id, promise);
    return promise;
  }

  function renderCurrentScreen() {
    const original = sourcePixels.get(currentScreen);
    if (!original) return;
    screenContext.putImageData(recolorImage(currentScreen, original), 0, 0);
  }

  function updateScreenDetails() {
    const screen = byId.get(currentScreen);
    if (!screen) return;
    activeTitle.textContent = screen.title;
    activeDescription.textContent = screen.description;
    activeIndex.innerHTML = `${String(screen.index + 1).padStart(2, "0")} <i>/</i> ${String(screenDefinitions.length).padStart(2, "0")}`;

    document.querySelectorAll("[data-screen]").forEach((button) => {
      const active = button.dataset.screen === currentScreen;
      button.classList.toggle("is-active", active);
      button.setAttribute("aria-pressed", String(active));
    });

    screenCanvas.setAttribute("aria-label", `Sonix Player ${screen.title} screen at 240 by 320 pixels. Tap the screen to interact.`);
  }

  function setScreen(screenId, { record = true } = {}) {
    if (!byId.has(screenId) || screenId === currentScreen) return;
    if (record && currentScreen !== "home" && screenId !== "home") history.push(currentScreen);
    if (record && currentScreen === "home" && screenId !== "home") history.push("home");
    if (screenId === "home") history = [];

    currentScreen = screenId;
    updateScreenDetails();
    screenWindow.classList.add("is-transitioning");
    prepareScreen(byId.get(screenId)).then(() => {
      renderCurrentScreen();
      window.setTimeout(() => screenWindow.classList.remove("is-transitioning"), 85);
    });
  }

  function goBack() {
    if (history.length) {
      const previous = history.pop();
      setScreen(previous, { record: false });
    } else if (currentScreen !== "home") {
      setScreen("home", { record: false });
    } else {
      showToast("You’re already on the home screen.");
    }
  }

  function showToast(message) {
    toast.textContent = message;
    toast.classList.add("is-visible");
    window.clearTimeout(toastTimer);
    toastTimer = window.setTimeout(() => toast.classList.remove("is-visible"), 2200);
  }

  function updatePlayback() {
    playStateLabel.textContent = isPlaying ? "PLAYING" : "PAUSED";
    playStateDot.classList.toggle("is-paused", !isPlaying);
    document.querySelectorAll(".key-play").forEach((button) => {
      const icon = button.querySelector(".mini-play-icon");
      if (icon) icon.classList.toggle("is-paused", !isPlaying);
      button.setAttribute("aria-label", isPlaying ? "Pause playback" : "Resume playback");
    });
  }

  function updateVolume() {
    volumeValueLabel.textContent = String(volume).padStart(2, "0");
    volumeFill.style.width = `${volume}%`;
    showToast(`Simulated volume · ${volume}%`);
  }

  function togglePlayback() {
    isPlaying = !isPlaying;
    updatePlayback();
    showToast(isPlaying ? "Playback resumed." : "Playback paused.");
  }

  function toggleScreenPower() {
    screenIsOn = !screenIsOn;
    screenOff.hidden = screenIsOn;
    screenCanvas.setAttribute("aria-label", screenIsOn ? "Sonix Player screen mockup. Tap the home tiles to navigate." : "Display asleep. Press Power or tap to wake.");
    showToast(screenIsOn ? "Display awake." : "Display asleep.");
  }

  function updateVolumeBy(delta) {
    volume = Math.max(0, Math.min(100, volume + delta));
    updateVolume();
  }

  function handleHardwareKey(action) {
    if (action === "power") {
      toggleScreenPower();
    } else if (action === "volume-up") {
      updateVolumeBy(1);
    } else if (action === "volume-down") {
      updateVolumeBy(-1);
    } else if (action === "play") {
      togglePlayback();
    } else if (action === "next") {
      showToast("Next-track key · playback controls are simulated.");
    }
  }

  function toCanvasPoint(event) {
    const rect = screenCanvas.getBoundingClientRect();
    return {
      x: Math.floor((event.clientX - rect.left) * WIDTH / rect.width),
      y: Math.floor((event.clientY - rect.top) * HEIGHT / rect.height),
    };
  }

  function handleScreenTap(event) {
    if (!screenIsOn) {
      toggleScreenPower();
      return;
    }

    const { x, y } = toCanvasPoint(event);
    if (currentScreen === "home") {
      if (y >= 91 && y <= 146) setScreen(x < 120 ? "music" : "streaming");
      else if (y >= 150 && y <= 205) setScreen(x < 120 ? "wireless" : "more");
      else if (y >= 207 && y <= 266) setScreen(x < 120 ? "more" : "settings");
      else if (y >= 270) setScreen("nowPlaying");
      return;
    }

    if (x <= 43 && y <= 67) {
      goBack();
      return;
    }

    if (currentScreen === "music") {
      if (y >= 100) setScreen("files");
    } else if (currentScreen === "more") {
      if (y >= 143 && y <= 207) setScreen("files");
    } else if (currentScreen === "files") {
      if (y >= 73) setScreen("nowPlaying");
    } else if (currentScreen === "nowPlaying" && y >= 270) {
      if (x >= 96 && x <= 145) togglePlayback();
      else if (x < 96 || x > 145) showToast("Track-skip control · simulated in this preview.");
    } else if (currentScreen === "settings" && y >= 78 && y <= 150) {
      showToast("Appearance → Theme · previewed with the amber palette at right.");
    }
  }

  document.querySelectorAll("[data-screen]").forEach((button) => {
    button.addEventListener("click", () => setScreen(button.dataset.screen));
  });

  document.querySelectorAll("[data-key]").forEach((button) => {
    button.addEventListener("click", () => handleHardwareKey(button.dataset.key));
  });

  screenCanvas.addEventListener("click", handleScreenTap);
  screenOff.addEventListener("click", toggleScreenPower);
  screenCanvas.addEventListener("keydown", (event) => {
    if (event.key === "Enter") handleScreenTap({ clientX: screenCanvas.getBoundingClientRect().left + screenCanvas.clientWidth / 2, clientY: screenCanvas.getBoundingClientRect().top + screenCanvas.clientHeight / 2 });
  });

  preserveArtToggle.addEventListener("change", renderCurrentScreen);

  window.addEventListener("keydown", (event) => {
    const target = event.target;
    if (target instanceof HTMLElement && (target.isContentEditable || ["BUTTON", "INPUT", "TEXTAREA", "SELECT"].includes(target.tagName))) return;

    if (event.code === "Space") {
      event.preventDefault();
      togglePlayback();
    } else if (event.key === "Escape" || event.key === "Backspace" || event.key === "ArrowLeft") {
      event.preventDefault();
      goBack();
    } else if (event.key === "Home") {
      event.preventDefault();
      setScreen("home");
    } else if (event.key === "ArrowRight") {
      event.preventDefault();
      const next = (byId.get(currentScreen).index + 1) % screenDefinitions.length;
      setScreen(screenDefinitions[next].id);
    }
  });

  screenDefinitions.forEach(prepareScreen);
  updateScreenDetails();
  updatePlayback();
  volumeFill.style.width = `${volume}%`;
  volumeValueLabel.textContent = String(volume).padStart(2, "0");
})();
