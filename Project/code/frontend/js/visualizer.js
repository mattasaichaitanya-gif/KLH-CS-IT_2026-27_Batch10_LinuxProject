/**
 * @file visualizer.js
 * @brief High-DPI Clean Canvas Renderer for Circular Bounded Buffer Structure
 * POSIX Concurrency Visualizer
 */

class CircularBufferVisualizer {
  constructor(canvasId) {
    this.canvas = document.getElementById(canvasId);
    if (!this.canvas) return;
    this.ctx = this.canvas.getContext('2d');
    this.particles = [];
    this.lastSlotAffected = -1;
    this.lastActionType = null;
    this.resizeObserver = null;

    this.setupHighDPI();
    this.initResizeObserver();
  }

  initResizeObserver() {
    if (window.ResizeObserver && this.canvas.parentElement) {
      this.resizeObserver = new ResizeObserver(() => {
        this.setupHighDPI();
      });
      this.resizeObserver.observe(this.canvas.parentElement);
    } else {
      window.addEventListener('resize', () => this.setupHighDPI());
    }
  }

  setupHighDPI() {
    if (!this.canvas) return;
    const parent = this.canvas.parentElement;
    const rect = parent ? parent.getBoundingClientRect() : this.canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;

    // Compact 210px presentation height for clean 1-screen fit
    const displayWidth = Math.max(280, Math.floor(rect.width || 520));
    const displayHeight = 210;

    this.canvas.width = Math.round(displayWidth * dpr);
    this.canvas.height = Math.round(displayHeight * dpr);
    this.canvas.style.width = `${displayWidth}px`;
    this.canvas.style.height = `${displayHeight}px`;

    this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    this.width = displayWidth;
    this.height = displayHeight;
  }

  triggerPulse(slotIndex, actionType) {
    this.lastSlotAffected = slotIndex;
    this.lastActionType = actionType;

    const centerX = this.width / 2;
    const centerY = this.height / 2;
    const minDim = Math.min(centerX, centerY);
    const midRadius = minDim * 0.52;

    const capacity = window.systemSim ? window.systemSim.capacity : 5;
    const angle = (2 * Math.PI * (slotIndex + 0.5) / capacity) - Math.PI / 2;
    const slotX = centerX + midRadius * Math.cos(angle);
    const slotY = centerY + midRadius * Math.sin(angle);

    const color = actionType === 'produce' ? '#10b981' : '#a78bfa';

    for (let i = 0; i < 12; i++) {
      const pAngle = Math.random() * 2 * Math.PI;
      const speed = 1.0 + Math.random() * 2.0;
      this.particles.push({
        x: slotX,
        y: slotY,
        vx: Math.cos(pAngle) * speed,
        vy: Math.sin(pAngle) * speed,
        alpha: 1.0,
        color: color,
        size: 2.0 + Math.random() * 2.0
      });
    }
  }

  render(bufferState) {
    if (!this.ctx || !this.width || !this.height) return;
    const ctx = this.ctx;
    const width = this.width;
    const height = this.height;

    const centerX = width / 2;
    const centerY = height / 2;
    const capacity = bufferState.capacity || 5;
    const count = bufferState.count || 0;
    const head = bufferState.head || 0;
    const tail = bufferState.tail || 0;
    const slots = bufferState.slots || [];

    ctx.clearRect(0, 0, width, height);

    // Dimensions: ample safe buffer so pointers and badges NEVER touch canvas edges
    const safeRadius = Math.min(centerX, centerY) - 30;
    const outerRadius = Math.max(50, safeRadius);
    const trackWidth = Math.max(24, Math.min(outerRadius * 0.35, 34));
    const innerRadius = outerRadius - trackWidth;
    const midRadius = (outerRadius + innerRadius) / 2;

    // 1. Background Ring Track
    ctx.save();
    ctx.beginPath();
    ctx.arc(centerX, centerY, outerRadius + 2, 0, 2 * Math.PI);
    ctx.arc(centerX, centerY, innerRadius - 2, 0, 2 * Math.PI, true);
    ctx.fillStyle = 'rgba(15, 23, 42, 0.5)';
    ctx.fill();

    ctx.beginPath();
    ctx.arc(centerX, centerY, outerRadius + 2, 0, 2 * Math.PI);
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
    ctx.lineWidth = 1;
    ctx.stroke();

    ctx.beginPath();
    ctx.arc(centerX, centerY, innerRadius - 2, 0, 2 * Math.PI);
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
    ctx.lineWidth = 1;
    ctx.stroke();
    ctx.restore();

    // 2. Buffer Slots
    const angleStep = (2 * Math.PI) / capacity;
    const gapAngle = Math.min(0.04, 0.16 / capacity);

    for (let i = 0; i < capacity; i++) {
      const startAngle = i * angleStep - Math.PI / 2 + gapAngle;
      const endAngle = (i + 1) * angleStep - Math.PI / 2 - gapAngle;
      const midAngle = i * angleStep - Math.PI / 2 + angleStep / 2;

      const item = slots[i];
      const isFilled = item !== null && item !== undefined;

      ctx.beginPath();
      ctx.arc(centerX, centerY, outerRadius, startAngle, endAngle);
      ctx.arc(centerX, centerY, innerRadius, endAngle, startAngle, true);
      ctx.closePath();

      if (isFilled) {
        // Glowing cyan sector
        const grad = ctx.createRadialGradient(centerX, centerY, innerRadius, centerX, centerY, outerRadius);
        grad.addColorStop(0, 'rgba(14, 165, 233, 0.25)');
        grad.addColorStop(1, 'rgba(56, 189, 248, 0.45)');
        ctx.fillStyle = grad;
        ctx.strokeStyle = '#38bdf8';
        ctx.lineWidth = 1.5;
      } else {
        // Clean subtle dark slot outline - no unwanted text!
        ctx.fillStyle = 'rgba(24, 33, 54, 0.35)';
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.07)';
        ctx.lineWidth = 1;
      }

      ctx.fill();
      ctx.stroke();

      const slotX = centerX + midRadius * Math.cos(midAngle);
      const slotY = centerY + midRadius * Math.sin(midAngle);

      // Content inside Slot
      if (isFilled) {
        ctx.save();
        ctx.beginPath();
        const orbRadius = Math.min(10, trackWidth * 0.32);
        ctx.arc(slotX, slotY - 2, orbRadius, 0, 2 * Math.PI);
        ctx.fillStyle = '#0284c7';
        ctx.shadowColor = '#38bdf8';
        ctx.shadowBlur = 6;
        ctx.fill();
        ctx.restore();

        ctx.fillStyle = '#ffffff';
        ctx.font = 'bold 8.5px "JetBrains Mono", monospace';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(`#${item.id}`, slotX, slotY - 2);

        ctx.fillStyle = '#bae6fd';
        ctx.font = '600 7px Inter, sans-serif';
        ctx.fillText(`P${item.producer_id}`, slotX, slotY + 7);
      } else {
        // Subtle slot index inside the empty track - no clutter outside!
        ctx.fillStyle = 'rgba(148, 163, 184, 0.45)';
        ctx.font = '600 9px "JetBrains Mono", monospace';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(`${i}`, slotX, slotY);
      }
    }

    // 3. Center Hub (Clean Minimal Occupancy Readout)
    ctx.save();
    ctx.beginPath();
    ctx.arc(centerX, centerY, innerRadius - 5, 0, 2 * Math.PI);
    const gradHub = ctx.createRadialGradient(centerX, centerY, 0, centerX, centerY, innerRadius - 5);
    gradHub.addColorStop(0, '#172033');
    gradHub.addColorStop(1, '#0b1120');
    ctx.fillStyle = gradHub;
    ctx.fill();
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
    ctx.lineWidth = 1.2;
    ctx.stroke();

    const occupancyRatio = capacity > 0 ? count / capacity : 0;
    if (occupancyRatio > 0) {
      ctx.beginPath();
      ctx.arc(centerX, centerY, innerRadius - 8, -Math.PI / 2, -Math.PI / 2 + occupancyRatio * 2 * Math.PI);
      ctx.strokeStyle = count === capacity ? '#f43f5e' : '#38bdf8';
      ctx.lineWidth = 2.5;
      ctx.lineCap = 'round';
      ctx.stroke();
    }

    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillStyle = '#f8fafc';
    ctx.font = 'bold 15px "JetBrains Mono", monospace';
    ctx.fillText(`${count} / ${capacity}`, centerX, centerY);
    ctx.restore();

    // 4. Clean HEAD & TAIL Pointers (Unified when at same slot to prevent collision)
    const headAngle = head * angleStep - Math.PI / 2 + angleStep / 2;
    const tailAngle = tail * angleStep - Math.PI / 2 + angleStep / 2;

    if (head === tail) {
      this.drawCombinedPointer(centerX, centerY, outerRadius, headAngle);
    } else {
      this.drawPointer(centerX, centerY, outerRadius, headAngle, '#a78bfa', 'HEAD');
      this.drawPointer(centerX, centerY, outerRadius, tailAngle, '#34d399', 'TAIL');
    }

    // 5. Micro Particles
    for (let i = this.particles.length - 1; i >= 0; i--) {
      const p = this.particles[i];
      p.x += p.vx;
      p.y += p.vy;
      p.alpha -= 0.05;
      p.size = Math.max(0.5, p.size * 0.94);

      if (p.alpha <= 0) {
        this.particles.splice(i, 1);
        continue;
      }

      ctx.save();
      ctx.beginPath();
      ctx.arc(p.x, p.y, p.size, 0, 2 * Math.PI);
      ctx.fillStyle = p.color;
      ctx.globalAlpha = p.alpha;
      ctx.fill();
      ctx.restore();
    }
  }

  drawCombinedPointer(centerX, centerY, outerRadius, angle) {
    const ctx = this.ctx;
    const arrowTargetDist = outerRadius + 2;
    const pointerStartDist = outerRadius + 9;
    const badgeDist = outerRadius + 18;

    const targetX = centerX + arrowTargetDist * Math.cos(angle);
    const targetY = centerY + arrowTargetDist * Math.sin(angle);
    const startX = centerX + pointerStartDist * Math.cos(angle);
    const startY = centerY + pointerStartDist * Math.sin(angle);

    ctx.save();
    ctx.beginPath();
    ctx.moveTo(startX, startY);
    ctx.lineTo(targetX, targetY);
    ctx.strokeStyle = '#38bdf8';
    ctx.lineWidth = 1.6;
    ctx.stroke();

    const headLen = 3.5;
    const angleLeft = angle - Math.PI / 6;
    const angleRight = angle + Math.PI / 6;
    ctx.beginPath();
    ctx.moveTo(targetX, targetY);
    ctx.lineTo(targetX + headLen * Math.cos(angleLeft + Math.PI), targetY + headLen * Math.sin(angleLeft + Math.PI));
    ctx.moveTo(targetX, targetY);
    ctx.lineTo(targetX + headLen * Math.cos(angleRight + Math.PI), targetY + headLen * Math.sin(angleRight + Math.PI));
    ctx.strokeStyle = '#38bdf8';
    ctx.lineWidth = 1.6;
    ctx.stroke();

    const badgeX = centerX + badgeDist * Math.cos(angle);
    const badgeY = centerY + badgeDist * Math.sin(angle);

    ctx.font = 'bold 7px "JetBrains Mono", monospace';
    const pillW = 60;
    const pillH = 13;

    ctx.fillStyle = 'rgba(11, 17, 32, 0.95)';
    ctx.strokeStyle = 'rgba(56, 189, 248, 0.5)';
    ctx.lineWidth = 1;
    this.roundRect(ctx, badgeX - pillW / 2, badgeY - pillH / 2, pillW, pillH, 3, true, true);

    ctx.textBaseline = 'middle';
    ctx.textAlign = 'left';
    ctx.fillStyle = '#a78bfa';
    ctx.fillText('HEAD', badgeX - pillW / 2 + 5, badgeY);

    ctx.textAlign = 'center';
    ctx.fillStyle = '#64748b';
    ctx.fillText('·', badgeX, badgeY);

    ctx.textAlign = 'right';
    ctx.fillStyle = '#34d399';
    ctx.fillText('TAIL', badgeX + pillW / 2 - 5, badgeY);
    ctx.restore();
  }

  drawPointer(centerX, centerY, outerRadius, angle, color, label) {
    const ctx = this.ctx;
    const arrowTargetDist = outerRadius + 2;
    const pointerStartDist = outerRadius + 9;
    const badgeDist = outerRadius + 18;

    const targetX = centerX + arrowTargetDist * Math.cos(angle);
    const targetY = centerY + arrowTargetDist * Math.sin(angle);
    const startX = centerX + pointerStartDist * Math.cos(angle);
    const startY = centerY + pointerStartDist * Math.sin(angle);

    ctx.save();
    ctx.beginPath();
    ctx.moveTo(startX, startY);
    ctx.lineTo(targetX, targetY);
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.6;
    ctx.stroke();

    const headLen = 3.5;
    const angleLeft = angle - Math.PI / 6;
    const angleRight = angle + Math.PI / 6;
    ctx.beginPath();
    ctx.moveTo(targetX, targetY);
    ctx.lineTo(targetX + headLen * Math.cos(angleLeft + Math.PI), targetY + headLen * Math.sin(angleLeft + Math.PI));
    ctx.moveTo(targetX, targetY);
    ctx.lineTo(targetX + headLen * Math.cos(angleRight + Math.PI), targetY + headLen * Math.sin(angleRight + Math.PI));
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.6;
    ctx.stroke();

    const badgeX = centerX + badgeDist * Math.cos(angle);
    const badgeY = centerY + badgeDist * Math.sin(angle);

    ctx.font = 'bold 7px "JetBrains Mono", monospace';
    const textWidth = ctx.measureText(label).width;
    const pillW = textWidth + 6;
    const pillH = 12;

    ctx.fillStyle = 'rgba(11, 17, 32, 0.95)';
    ctx.strokeStyle = color;
    ctx.lineWidth = 1;
    this.roundRect(ctx, badgeX - pillW / 2, badgeY - pillH / 2, pillW, pillH, 3, true, true);

    ctx.fillStyle = color;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(label, badgeX, badgeY);
    ctx.restore();
  }

  roundRect(ctx, x, y, width, height, radius, fill, stroke) {
    ctx.beginPath();
    ctx.moveTo(x + radius, y);
    ctx.lineTo(x + width - radius, y);
    ctx.quadraticCurveTo(x + width, y, x + width, y + radius);
    ctx.lineTo(x + width, y + height - radius);
    ctx.quadraticCurveTo(x + width, y + height, x + width - radius, y + height);
    ctx.lineTo(x + radius, y + height);
    ctx.quadraticCurveTo(x, y + height, x, y + height - radius);
    ctx.lineTo(x, y + radius);
    ctx.quadraticCurveTo(x, y, x + radius, y);
    ctx.closePath();
    if (fill) ctx.fill();
    if (stroke) ctx.stroke();
  }
}

window.CircularBufferVisualizer = CircularBufferVisualizer;
