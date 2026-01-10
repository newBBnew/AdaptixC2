import React, { useEffect, useState } from 'react';
import { createPortal } from 'react-dom';
import { X } from 'lucide-react';
import { motion, AnimatePresence, useDragControls } from 'framer-motion';
import { cn } from '../utils/cn';

const Modal = ({ isOpen, onClose, title, children, width = 'max-w-4xl', height = 'auto' }) => {
  const dragControls = useDragControls();
  const [mounted, setMounted] = useState(false);

  useEffect(() => {
    setMounted(true);
    return () => setMounted(false);
  }, []);

  useEffect(() => {
    const handleEsc = (e) => {
      if (e.key === 'Escape') onClose();
    };
    if (isOpen) window.addEventListener('keydown', handleEsc);
    return () => window.removeEventListener('keydown', handleEsc);
  }, [isOpen, onClose]);

  if (!mounted) return null;

  return createPortal(
    <AnimatePresence>
      {isOpen && (
        <div className="fixed inset-0 z-[9999] flex items-start justify-center pt-[10vh] p-4 bg-black/40 backdrop-blur-sm pointer-events-auto">
          <motion.div
            initial={{ opacity: 0, scale: 0.95, y: 20 }}
            animate={{ opacity: 1, scale: 1, y: 0 }}
            exit={{ opacity: 0, scale: 0.95, y: 20 }}
            drag
            dragListener={false}
            dragControls={dragControls}
            dragMomentum={false}
            dragElastic={0.1}
            className={cn(
              "border border-theme-glass-light rounded-2xl shadow-2xl flex flex-col relative overflow-hidden backdrop-blur-2xl",
              width,
              height === 'auto' ? 'max-h-[85vh]' : height
            )}
            style={{ 
              boxShadow: '0 25px 50px -12px rgba(0, 0, 0, 0.5)',
              backgroundColor: 'var(--glass-bg-modal)',
            }}
          >
            {/* Header - acts as drag handle */}
            <div 
              onPointerDown={(e) => dragControls.start(e)}
              className="flex items-center justify-between px-4 py-2 border-b border-theme-glass-light bg-theme-glass/50 shrink-0 cursor-grab active:cursor-grabbing touch-none select-none"
            >
              <h3 className="text-[10px] font-black uppercase tracking-[0.2em] text-theme-muted pointer-events-none">{title}</h3>
              <button
                onClick={onClose}
                onPointerDown={(e) => e.stopPropagation()} // Prevent drag when clicking close
                className="p-1.5 hover:bg-theme-glass-panel rounded-xl transition-all text-theme-muted hover:text-theme-primary"
              >
                <X size={18} />
              </button>
            </div>

            {/* Content Area - Child handles its own scrolling if needed */}
            <div className="flex-1 min-h-0 cursor-default relative z-0">
              {children}
            </div>
          </motion.div>
        </div>
      )}
    </AnimatePresence>,
    document.body
  );
};

export default Modal;
